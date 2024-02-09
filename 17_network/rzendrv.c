#include <linux/init.h>          // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>        // printk
#include <linux/module.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h> /* eth_type_trans */
#include <linux/ip.h>          /* struct iphdr */
#include <linux/tcp.h>         /* struct tcphdr */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rafal Zielinski");
MODULE_DESCRIPTION("example network loopback driver");
MODULE_VERSION("0.1");

#define NR_DEVICES	2
#define RZEN_TIMEOUT	5
#define RZEN_RX_INTR 0x0001
#define RZEN_TX_INTR 0x0002

static int lockup = 0;
static int use_napi = 1;
static void (*rzen_interrupt)(int, void *, struct pt_regs *);
int pool_size = 8;
struct net_device* rzen_devs[NR_DEVICES];

struct rzen_dev_priv {
	struct net_device_stats stats;
	int status;
	struct rzen_packet *ppool;
	struct rzen_packet *rx_queue;  /* List of incoming packets */
	int rx_int_enabled;
	int tx_packetlen;
	u8 *tx_packetdata;
	struct sk_buff *skb;
	spinlock_t lock;
	struct net_device *dev;
	struct napi_struct napi;
};

struct rzen_packet {
	struct rzen_packet *next;
	struct net_device *dev;
	int	datalen;
	u8 data[ETH_DATA_LEN];
};

// Enable and disable receive interrupts.
static void rzen_rx_ints(struct net_device *dev, int enable){
	struct rzen_dev_priv *priv = netdev_priv(dev);
	priv->rx_int_enabled = enable;
}

struct rzen_packet *rzen_get_tx_buffer(struct net_device *dev){
	struct rzen_dev_priv *priv = netdev_priv(dev);
	unsigned long flags;
	struct rzen_packet *pkt;
    
	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->ppool;
	if(!pkt) {
		printk (KERN_INFO "Out of Pool\n");
		return pkt;
	}
	priv->ppool = pkt->next;
	if (priv->ppool == NULL) {
		printk (KERN_INFO "Pool empty\n");
		netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

void rzen_enqueue_buf(struct net_device *dev, struct rzen_packet *pkt){
	unsigned long flags;
	struct rzen_dev_priv *priv = netdev_priv(dev);

	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->rx_queue;  /* FIXME - misorders packets */
	priv->rx_queue = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
}

struct rzen_packet *rzen_dequeue_buf(struct net_device *dev)
{
	struct rzen_dev_priv *priv = netdev_priv(dev);
	struct rzen_packet *pkt;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	pkt = priv->rx_queue;
	if (pkt != NULL)
		priv->rx_queue = pkt->next;
	spin_unlock_irqrestore(&priv->lock, flags);
	return pkt;
}

void rzen_release_buffer(struct rzen_packet *pkt)
{
	unsigned long flags;
	struct rzen_dev_priv *priv = netdev_priv(pkt->dev);
	
	spin_lock_irqsave(&priv->lock, flags);
	pkt->next = priv->ppool;
	priv->ppool = pkt;
	spin_unlock_irqrestore(&priv->lock, flags);
	if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
		netif_wake_queue(pkt->dev);
}

static int rzen_poll(struct napi_struct *napi, int budget){
	int npackets = 0;
	struct sk_buff *skb;
	struct rzen_dev_priv *priv = container_of(napi, struct rzen_dev_priv, napi);
	struct net_device *dev = priv->dev;
	struct rzen_packet *pkt;
    
	while (npackets < budget && priv->rx_queue) {
		pkt = rzen_dequeue_buf(dev);
		skb = dev_alloc_skb(pkt->datalen + 2);
		if (! skb) {
			if (printk_ratelimit())
				printk(KERN_NOTICE "snull: packet dropped\n");
			priv->stats.rx_dropped++;
			npackets++;
			rzen_release_buffer(pkt);
			continue;
		}
		skb_reserve(skb, 2); /* align IP on 16B boundary */  
		memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
		skb->dev = dev;
		skb->protocol = eth_type_trans(skb, dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
		netif_receive_skb(skb);
		
        	/* Maintain stats */
		npackets++;
		priv->stats.rx_packets++;
		priv->stats.rx_bytes += pkt->datalen;
		rzen_release_buffer(pkt);
	}
	/* If we processed all packets, we're done; tell the kernel and reenable ints */
	//if (! priv->rx_queue) {
	if (npackets < budget) {
		unsigned long flags;
		spin_lock_irqsave(&priv->lock, flags);
		if (napi_complete_done(napi, npackets))
			rzen_rx_ints(dev, 1);
		spin_unlock_irqrestore(&priv->lock, flags);
		//return 0; // fall in return packets
	}
	/* We couldn't process everything. */
	return npackets;
}



int rzen_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, const void *daddr, const void *saddr,
                unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

	eth->h_proto = htons(type);
	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN-1]   ^= 0x01;   /* dest is us xor 1 */
	return (dev->hard_header_len);
}

static const struct header_ops rzen_header_ops = {
        .create  = rzen_header,
};

static void my_hw_tx(char *buf, int len, struct net_device *dev)
{
	/*
	 * This function deals with hw details. This interface loops
	 * back the packet to the other snull interface (if any).
	 * In other words, this function implements the snull behaviour,
	 * while all other procedures are rather device-independent
	 */
	struct iphdr *ih;
	struct net_device *dest;
	struct rzen_dev_priv *priv;
	u32 *saddr, *daddr;
	struct rzen_packet *tx_buffer;
    
	/* I am paranoid. Ain't I? */
	if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) {
		printk("snull: Hmm... packet too short (%i octets)\n",
				len);
		return;
	}

	if (0) { /* enable this conditional to look at the data */
		int i;
		printk (KERN_INFO "len is %i\n" KERN_DEBUG "data:",len);
		for (i=14 ; i<len; i++)
			printk(" %02x",buf[i]&0xff);
		printk("\n");
	}
	/*
	 * Ethhdr is 14 bytes, but the kernel arranges for iphdr
	 * to be aligned (i.e., ethhdr is unaligned)
	 */
	ih = (struct iphdr *)(buf+sizeof(struct ethhdr));
	saddr = &ih->saddr;
	daddr = &ih->daddr;

	((u8 *)saddr)[2] ^= 1; /* change the third octet (class C) */
	((u8 *)daddr)[2] ^= 1;

	ih->check = 0;         /* and rebuild the checksum (ip needs it) */
	ih->check = ip_fast_csum((unsigned char *)ih,ih->ihl);

	if (dev == rzen_devs[0])
		printk (KERN_INFO "%08x:%05i --> %08x:%05i\n",
				ntohl(ih->saddr),ntohs(((struct tcphdr *)(ih+1))->source),
				ntohl(ih->daddr),ntohs(((struct tcphdr *)(ih+1))->dest));
	else
		printk (KERN_INFO "%08x:%05i <-- %08x:%05i\n",
				ntohl(ih->daddr),ntohs(((struct tcphdr *)(ih+1))->dest),
				ntohl(ih->saddr),ntohs(((struct tcphdr *)(ih+1))->source));

	/*
	 * Ok, now the packet is ready for transmission: first simulate a
	 * receive interrupt on the twin device, then  a
	 * transmission-done on the transmitting device
	 */
	dest = rzen_devs[dev == rzen_devs[0] ? 1 : 0];
	priv = netdev_priv(dest);
	tx_buffer = rzen_get_tx_buffer(dev);

	if(!tx_buffer) {
		printk (KERN_INFO "Out of tx buffer, len is %i\n",len);
		return;
	}

	tx_buffer->datalen = len;
	memcpy(tx_buffer->data, buf, len);
	rzen_enqueue_buf(dest, tx_buffer);
	if (priv->rx_int_enabled) {
		priv->status |= RZEN_RX_INTR;
		rzen_interrupt(0, dest, NULL);
	}

	priv = netdev_priv(dev);
	priv->tx_packetlen = len;
	priv->tx_packetdata = buf;
	priv->status |= RZEN_TX_INTR;
	if (lockup && ((priv->stats.tx_packets + 1) % lockup) == 0) {
        	/* Simulate a dropped transmit interrupt */
		netif_stop_queue(dev);
		printk (KERN_INFO "Simulate lockup at %ld, txp %ld\n", jiffies,
				(unsigned long) priv->stats.tx_packets);
	}
	else
		rzen_interrupt(0, dev, NULL);
}

int rzen_tx(struct sk_buff *skb, struct net_device *dev)
{
	int len;
	char *data, shortpkt[ETH_ZLEN];
	struct rzen_dev_priv *priv = netdev_priv(dev);
	//printk(KERN_INFO "rzen: tx device \"%s\"\n", dev->name);

	data = skb->data;
	len = skb->len;
	if (len < ETH_ZLEN) {
		memset(shortpkt, 0, ETH_ZLEN);
		memcpy(shortpkt, skb->data, skb->len);
		len = ETH_ZLEN;
		data = shortpkt;
	}
	netif_trans_update(dev);

	/* Remember the skb, so we can free it at interrupt time */
	priv->skb = skb;

	/* actual deliver of data is device-specific, and not shown here */
	my_hw_tx(data, len, dev);

	return 0; /* Our simple device can not fail */
}

int rzen_open(struct net_device *dev){
	/* request_region(), request_irq(), ....  (like fops->open) */
	
	printk(KERN_INFO "rzen: opening device \"%s\"\n", dev->name);
	
	/* 
	 * Assign the hardware address of the board: use "\0SNULx", where
	 * x is 0 or 1. The first byte is '\0' to avoid being a multicast
	 * address (the first byte of multicast addrs is odd).
	 */
	memcpy(dev->dev_addr, "\0RZEN0", ETH_ALEN);
	if (dev == rzen_devs[1])
		dev->dev_addr[ETH_ALEN-1]++; /* \0RZEN1 */
	if (use_napi) {
		struct rzen_dev_priv *priv = netdev_priv(dev);
		napi_enable(&priv->napi);
	}
	netif_start_queue(dev);
	return 0;
}

int rzen_release(struct net_device *dev){
	/* release ports, irq and such -- like fops->close */
	printk(KERN_INFO "rzen: releasing device \"%s\"\n", dev->name);

	netif_stop_queue(dev); /* can't transmit any more */
        if (use_napi) {
                struct rzen_dev_priv *priv = netdev_priv(dev);
                napi_disable(&priv->napi);
        }
	return 0;
}

int rzen_ioctl(struct net_device *dev, struct ifreq *rq, int cmd){
	printk(KERN_INFO "rzen: ioctl\n");
	return 0;
}

// Configuration changes (passed on by ifconfig)
int rzen_config(struct net_device *dev, struct ifmap *map){
	printk(KERN_INFO "rzen: config device \"%s\"\n", dev->name);
	if (dev->flags & IFF_UP) /* can't act on a running interface */
		return -EBUSY;

	/* Don't allow changing the I/O address */
	if (map->base_addr != dev->base_addr) {
		printk(KERN_WARNING "snull: Can't change I/O address\n");
		return -EOPNOTSUPP;
	}

	/* Allow changing the IRQ */
	if (map->irq != dev->irq) {
		dev->irq = map->irq;
        	/* request_irq() is delayed to open-time */
	}

	/* ignore other fields */
	return 0;
}

// Return statistics to the caller
struct net_device_stats *rzen_stats(struct net_device *dev){
	struct rzen_dev_priv *priv = netdev_priv(dev);
	//printk(KERN_INFO "rzen: stats device \"%s\"\n", dev->name);
	return &priv->stats;
}


//Clear a device's packet pool.
void rzen_teardown_pool(struct net_device *dev){
	struct rzen_dev_priv *priv = netdev_priv(dev);
	struct rzen_packet *pkt;
    
	while ((pkt = priv->ppool)) {
		priv->ppool = pkt->next;
		kfree (pkt);
	}
}


//Set up a device's packet pool.
void rzen_setup_pool(struct net_device *dev){
	struct rzen_dev_priv *priv = netdev_priv(dev);
	int i;
	struct rzen_packet *pkt;

	priv->ppool = NULL;
	for (i = 0; i < pool_size; i++) {
		pkt = kmalloc (sizeof (struct rzen_packet), GFP_KERNEL);
		if (pkt == NULL) {
			printk (KERN_NOTICE "Ran out of memory allocating packet pool\n");
			return;
		}
		pkt->dev = dev;
		pkt->next = priv->ppool;
		priv->ppool = pkt;
	}
}

void rzen_tx_timeout (struct net_device *dev, unsigned int txqueue){
	struct rzen_dev_priv *priv = netdev_priv(dev);
	struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);

	printk(KERN_INFO "Transmit timeout at %ld, latency %ld\n", jiffies,
			jiffies - txq->trans_start);
	/* Simulate a transmission interrupt to get things moving */
	priv->status |= RZEN_TX_INTR;
	rzen_interrupt(0, dev, NULL);
	priv->stats.tx_errors++;

	/* Reset packet pool */
	spin_lock(&priv->lock);
	rzen_teardown_pool(dev);
	rzen_setup_pool(dev);
	spin_unlock(&priv->lock);

	netif_wake_queue(dev);
	return;
}

static const struct net_device_ops rzen_netdev_ops = {
	.ndo_open            = rzen_open,
	.ndo_stop            = rzen_release,
	.ndo_start_xmit      = rzen_tx,
	.ndo_do_ioctl        = rzen_ioctl,
	.ndo_set_config      = rzen_config,
	.ndo_get_stats       = rzen_stats,
	.ndo_tx_timeout      = rzen_tx_timeout,
};

static void rzen_napi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	int statusword;
	struct rzen_dev_priv *priv;

	/*
	 * As usual, check the "device" pointer for shared handlers.
	 * Then assign "struct device *dev"
	 */
	struct net_device *dev = (struct net_device *)dev_id;
	/* ... and check with hw if it's really ours */

	/* paranoid */
	if (!dev)
		return;

	/* Lock the device */
	priv = netdev_priv(dev);
	spin_lock(&priv->lock);

	/* retrieve statusword: real netdevices use I/O instructions */
	statusword = priv->status;
	priv->status = 0;
	if (statusword & RZEN_RX_INTR) {
		rzen_rx_ints(dev, 0);  /* Disable further interrupts */
		napi_schedule(&priv->napi);
	}
	if (statusword & RZEN_TX_INTR) {
        	/* a transmission is over: free the skb */
		priv->stats.tx_packets++;
		priv->stats.tx_bytes += priv->tx_packetlen;
		if(priv->skb) {
			dev_kfree_skb(priv->skb);
			priv->skb = 0;
		}
	}

	/* Unlock the device and we are done */
	spin_unlock(&priv->lock);
	return;
}

void rzen_init(struct net_device *dev){
	struct rzen_dev_priv *priv;
	
	ether_setup(dev); /* assign some of the fields */
	dev->watchdog_timeo = RZEN_TIMEOUT;
	dev->netdev_ops = &rzen_netdev_ops;
	dev->header_ops = &rzen_header_ops;
	/* keep the default flags, just add NOARP */
	dev->flags           |= IFF_NOARP;
	dev->features        |= NETIF_F_HW_CSUM;
	
	priv = netdev_priv(dev);
	memset(priv, 0, sizeof(struct rzen_dev_priv));
	if (use_napi) {
		netif_napi_add(dev, &priv->napi, rzen_poll,2);
	}
	spin_lock_init(&priv->lock);
	priv->dev = dev;

	rzen_rx_ints(dev, 1);		/* enable receive interrupts */
	rzen_setup_pool(dev);

}

static void __exit rzendrv_exit(void){
	int i;
	for (i = 0; i < NR_DEVICES;  i++) {
		if (rzen_devs[i]) {
			printk(KERN_INFO "rzen: clearing device \"%s\"\n", rzen_devs[i]->name);
			unregister_netdev(rzen_devs[i]);
			rzen_teardown_pool(rzen_devs[i]);
			free_netdev(rzen_devs[i]);
		}
	}
	printk(KERN_INFO "rzen LKM unloaded!\n");
	return;
}


static int __init rzendrv_init(void){
        
        int result, i, ret = -ENOMEM;
        rzen_interrupt = rzen_napi_interrupt;
        
        for (i = 0; i < NR_DEVICES;  i++){
        	printk(KERN_INFO "rzen: allocatting device %d\n",i);
        	rzen_devs[i] = alloc_netdev(sizeof(struct rzen_dev_priv), "rzend%d",
			NET_NAME_UNKNOWN, rzen_init);
        } 
        
	for (i = 0; i < NR_DEVICES;  i++){
		if (rzen_devs[i] == NULL){
			printk(KERN_INFO "rzen: error allocatting device %d\n",i);
			goto out;
		}
	}
	
	for (i = 0; i < NR_DEVICES;  i++){
		if ((result = register_netdev(rzen_devs[i])))
			printk(KERN_INFO "rzen: error %i registering device \"%s\"\n", result, rzen_devs[i]->name);
		else{
			printk(KERN_INFO "rzen: device \"%s\" registerred\n", rzen_devs[i]->name);
			ret = 0;
		}
	}
out:
	if (ret){
		for (i = 0; i < NR_DEVICES;  i++) {
			if (rzen_devs[i]) {
				free_netdev(rzen_devs[i]);
			}
		}
	}
	return ret;
}

module_init(rzendrv_init);
module_exit(rzendrv_exit);
