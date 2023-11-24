#include <asm/checksum.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/tcp.h>
#include <linux/types.h>

#define TX_INTERRUPT 0x1
#define RX_INTERRUPT 0x2

static int netdevice_open(struct net_device* dev);
static int netdevice_release(struct net_device* dev);
static struct netdevice_packet* netdevice_get_tx_buffer(struct net_device* dev);
static int netdevice_config(struct net_device* dev, struct ifmap* map);
static int netdevice_ioctl(struct net_device* dev, struct ifreq* ifr, int cmd);
static int netdevice_start_transmit(struct sk_buff* skb, struct net_device* dev);
static void netdevice_transmit_timeout(struct net_device* dev, unsigned int txqueue);
static struct net_device_stats* netdevice_get_stats(struct net_device* dev);
static int netdevice_header(struct sk_buff* skb, struct net_device* dev, unsigned short type, const void* daddr, const void* saddr, unsigned int len);

struct netdevice_packet
{
    struct netdevice_packet* next;
    struct net_device* dev;
    int datalen;
    u8 data[ETH_DATA_LEN];
};

struct netdevice_private
{
    struct sk_buff* skb;
    struct net_device_stats stats;
    int status;
    struct netdevice_packet* ppool;
    struct netdevice_packet* rx_queue;
    int rx_int_enabled;
    int tx_packetlen;
    u8 tx_packetdata;
    spinlock_t lock;
    struct net_device* dev;
    struct napi_struct napi;
};

static struct net_device* netdevs[2];

static const struct header_ops hops =
{
    .create = netdevice_header
};

static const struct net_device_ops nops =
{
    // .ndo_init = netdevice_init,
    .ndo_open = netdevice_open, //assign an address to network interface
    .ndo_stop = netdevice_release, // shutdown interface
    .ndo_start_xmit = netdevice_start_transmit, // start transmission of a packet
    .ndo_do_ioctl = netdevice_ioctl, // implementation of custom commands
    .ndo_get_stats = netdevice_get_stats, // stats
    .ndo_tx_timeout = netdevice_transmit_timeout // transmission packet timeout
};

static int netdevice_open(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Open\n");

    u8 fake_mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x00 };

    // Copy hardware MAC address to allow device to communicate
    // ETH_ALEN - just a string length of Ethernet hardware address

    memcpy(dev->dev_addr, fake_mac, ETH_ALEN);
    if (dev == netdevs[1])
        dev->dev_addr[ETH_ALEN - 1]++;

    struct netdevice_private* priv = netdev_priv(dev);
    napi_enable(&priv->napi);

    return 0;
}

static int netdevice_release(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Release\n");

    netif_stop_queue(dev);
    struct netdevice_private* priv = netdev_priv(dev);
    napi_disable(&priv->napi);

    return 0;
}

static int netdevice_config(struct net_device* dev, struct ifmap* map)
{
    printk(KERN_INFO "NetDriver: Config");

    return 0;
}

static int netdevice_ioctl(struct net_device* dev, struct ifreq* ifr, int cmd)
{
    printk(KERN_INFO "NetDriver: Ioctl\n");

    return 0;
}

static struct netdevice_packet* netdevice_get_tx_buffer(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Get transmission buffer\n");

    struct netdevice_private* priv = netdev_priv(dev);
    unsigned long flags;
    struct netdevice_packet* pkt;

    spin_lock_irqsave(&priv->lock, flags);
    pkt = priv->ppool;
    if (!pkt)
    {
        printk(KERN_INFO "NetDriver: Out of pool boundaries\n");
        return pkt;
    }

    priv->ppool = pkt->next;
    if (!priv->ppool)
    {
        printk(KERN_INFO "NetDriver: Pool empty\n");
        netif_stop_queue(dev);
    }

    spin_unlock_irqrestore(&priv->lock, flags);

    return pkt;
}

static void netdevice_set_receive_ints(struct net_device* dev, bool enable)
{
    printk(KERN_INFO "NetDriver: Set receive interrupts\n");

    struct netdevice_private* priv = netdev_priv(dev);
    priv->rx_int_enabled = enable;
}

static void netdevice_setup_pool(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Pool setup\n");

    struct netdevice_private* priv = netdev_priv(dev);
    int i;
    struct netdevice_packet* pkt;
    int pool_size = 10;

    priv->ppool = NULL;
    for (i = 0; i < pool_size; i++)
    {
        pkt = kmalloc(sizeof(struct netdevice_packet), GFP_KERNEL);
        if (!pkt)
        {
            printk(KERN_INFO "NetDriver: Packet allocation error\n");
            return;
        }
        pkt->dev = dev;
        pkt->next = priv->ppool;
        priv->ppool = pkt;
    }
}

static void netdevice_cleanup_pool(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Pool cleanup\n");

    struct netdevice_private* priv = netdev_priv(dev);
    struct netdevice_packet* pkt;

    while ((pkt = priv->ppool))
    {
        priv->ppool = pkt->next;
        kfree(pkt);
    }
}

static void netdevice_enqueue_buf(struct net_device* dev, struct netdevice_packet* pkt)
{
    printk(KERN_INFO "NetDriver: Enqueue buffer\n");

    unsigned long flags;
    struct netdevice_private* priv = netdev_priv(dev);

    spin_lock_irqsave(&priv->lock, flags);
    pkt->next = priv->rx_queue;
    priv->rx_queue = pkt;
    spin_unlock_irqrestore(&priv->lock, flags);
}

static void netdevice_interrupt(int irq, void* dev_id, struct pt_regs* regs)
{
    printk(KERN_INFO "NetDriver: Napi interrupt handler\n");

    int status;
    struct netdevice_private* priv;
    struct net_device* dev = (struct net_device*) dev_id;

    if (!dev)
        return;

    priv = netdev_priv(dev);
    spin_lock(&priv->lock);

    status = priv->status;
    priv->status = 0;

    if (status & RX_INTERRUPT)
    {
        netdevice_set_receive_ints(dev, 0);
        napi_schedule(&priv->napi);
    }

    if (status & TX_INTERRUPT)
    {
        priv->stats.tx_packets++;
        priv->stats.tx_bytes += priv->tx_packetlen;
        if (priv->skb)
        {
            dev_kfree_skb(priv->skb);
            priv->skb = 0;
        }
    }

    spin_unlock(&priv->lock);
}

static void netdevice_hw_transmit(struct net_device* dev, int len, char* buf)
{
    printk(KERN_INFO "NetDriver: Hardware transmit\n");

    struct iphdr* ih;
    struct net_device* dest;
    struct netdevice_private* priv;
    u32* saddr, * daddr;
    struct netdevice_packet* tx_buffer;

    if (len < sizeof(struct ethhdr) + sizeof(struct iphdr))
    {
        printk(KERN_INFO "NetDriver: Packet too short\n");
        return;
    }

    ih = (struct iphdr*)(buf + sizeof(struct ethhdr));
    saddr = &ih->saddr;
    daddr = &ih->daddr;

    if (dev == netdevs[0])
        printk(KERN_INFO "orig: %08x:%05i --> %08x:%05i\n", ntohl(ih->saddr),
                         ntohs(((struct tcphdr*)(ih + 1))->source),
                         ntohl(ih->daddr),
                         ntohs(((struct tcphdr*)(ih + 1))->dest));
    else
        printk(KERN_INFO "orig: %08x:%05i --> %08x:%05i\n", ntohl(ih->daddr),
                         ntohs(((struct tcphdr*)(ih + 1))->source),
                         ntohl(ih->saddr),
                         ntohs(((struct tcphdr*)(ih + 1))->dest));    

    // change networks
    ((u8*) saddr)[2] ^= 1;
    ((u8*) daddr)[2] ^= 1;

    // fix checksum after change of ip
    ih->check = 0;
    ih->check = ip_fast_csum((unsigned char*) ih, ih->ihl);

    if (dev == netdevs[0])
        printk(KERN_INFO "%08x:%05i --> %08x:%05i\n", ntohl(ih->saddr),
                         ntohs(((struct tcphdr*)(ih + 1))->source),
                         ntohl(ih->daddr),
                         ntohs(((struct tcphdr*)(ih + 1))->dest));

        printk(KERN_INFO "%08x:%05i --> %08x:%05i\n", ntohl(ih->saddr),
                         ntohs(((struct tcphdr*)(ih + 1))->source),
                         ntohl(ih->daddr),
                         ntohs(((struct tcphdr*)(ih + 1))->dest));

    dest = netdevs[dev ==netdevs[0] ? 1 : 0];
    priv = netdev_priv(dest);

    tx_buffer = netdevice_get_tx_buffer(dev);
    if (!tx_buffer)
    {
        printk(KERN_INFO "NetDriver: Buffer empty\n");
        return;
    }

    tx_buffer->datalen = len;
    memcpy(tx_buffer->data, buf, len);
    netdevice_enqueue_buf(dest, tx_buffer);
    if (priv->rx_int_enabled)
    {
        priv->status |= RX_INTERRUPT;
        netdevice_interrupt(0, dest, NULL);
    }

    priv = netdev_priv(dev);
    priv->tx_packetlen = len;
    priv->tx_packetdata = buf;
    priv->status |= TX_INTERRUPT;

    netdevice_interrupt(0, dev, NULL);
}

static int netdevice_start_transmit(struct sk_buff* skb, struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Transmit\n");

    int len;
    char* data, shortpkt[ETH_ZLEN];
    struct netdevice_private* priv = netdev_priv(dev);

    data = skb->data;
    len = skb->len;

    if (len < ETH_ZLEN)
    {
        memset(shortpkt, 0, ETH_ZLEN);
        memcpy(shortpkt, skb->data, skb->len);
        len = ETH_ZLEN;
        data = shortpkt;
    }

    priv->skb = skb;
    netdevice_hw_transmit(dev, len, data);
    
    return 0;
}

static struct net_device_stats* netdevice_get_stats(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Stats\n");

    struct netdevice_private*priv = netdev_priv(dev);
    return &priv->stats;
}

static int netdevice_header(struct sk_buff* skb, struct net_device* dev, unsigned short type, const void* daddr, const void* saddr, unsigned int len)
{
    printk(KERN_INFO "NetDriver: Build header\n");

    struct ethhdr* eth = (struct ethhdr*) skb_push(skb, ETH_HLEN);

    eth->h_proto = htons(type);
    memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
    memcpy(eth->h_dest, daddr ? daddr : dev->dev_addr, dev->addr_len);
    eth->h_dest[ETH_ALEN - 1] ^= 0x01;

    return dev->hard_header_len;
}

static void netdevice_transmit_timeout(struct net_device* dev, unsigned int txqueue)
{
    printk(KERN_INFO "NetDriver: Transmission timeout\n");

    struct netdevice_private* priv = netdev_priv(dev);
    struct netdev_queue* txq = netdev_get_tx_queue(dev, 0);

    priv->status |= TX_INTERRUPT;
    netdevice_interrupt(0, dev, NULL);
    priv->stats.tx_errors++;

    spin_lock(&priv->lock);
    netdevice_cleanup_pool(dev);
    netdevice_setup_pool(dev);
    spin_unlock(&priv->lock);

    netif_wake_queue(dev);
}

static struct netdevice_packet* netdevice_dequeue_buf(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Cleanup queue\n");

    struct netdevice_private* priv = netdev_priv(dev);
    struct netdevice_packet* pkt;
    unsigned long flags;

    spin_lock_irqsave(&priv->lock, flags);
    pkt = priv->rx_queue;
    if (pkt)
        priv->rx_queue = pkt->next;
    spin_unlock_irqrestore(&priv->lock, flags);

    return pkt;
}

static void netdevice_release_buffer(struct netdevice_packet* pkt)
{
    printk(KERN_INFO "NetDriver: Release buffer\n");

    unsigned long flags;
    struct netdevice_private* priv = netdev_priv(pkt->dev);

    spin_lock_irqsave(&priv->lock, flags);
    pkt->next = priv->ppool;
    priv->ppool = pkt;
    spin_unlock_irqrestore(&priv->lock, flags);

    if (netif_queue_stopped(pkt->dev) && !pkt->next)
        netif_wake_queue(pkt->dev);
}

static int netdevice_poll(struct napi_struct* napi, int budget)
{
    printk(KERN_INFO "NetDriver: Napi poll\n");

    int npackets = 0;
    struct sk_buff* skb;
    struct netdevice_private* priv = container_of(napi, struct netdevice_private, napi);
    struct net_device* dev = priv->dev;
    struct netdevice_packet* pkt;

    while (npackets < budget && priv->rx_queue)
    {
        pkt = netdevice_dequeue_buf(dev);
        skb = dev_alloc_skb(pkt->datalen + 2); // ???

        if (!skb)
        {
            printk(KERN_INFO "NetDriver: Packet allocation error\n");
            priv->stats.rx_dropped++;
            netdevice_release_buffer(pkt);
            continue;
        }

        memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
        skb->dev = dev;
        skb->protocol = eth_type_trans(skb, dev);
        skb->ip_summed = CHECKSUM_UNNECESSARY;
        netif_receive_skb(skb);

        npackets++;
        priv->stats.rx_packets++;
        priv->stats.rx_bytes += pkt->datalen;
        netdevice_release_buffer(pkt);
    }

    if (npackets < budget)
    {
        unsigned long flags;
        spin_lock_irqsave(&priv->lock, flags);
        napi_complete(napi);
        netdevice_set_receive_ints(dev, 1);
        spin_unlock_irqrestore(&priv->lock, flags);
    }
    
    return npackets;
}

static void netdev_init(struct net_device* dev)
{
    printk(KERN_INFO "NetDriver: Setup\n");

    ether_setup(dev);

    dev->netdev_ops = &nops;
    dev->watchdog_timeo = 10;
    dev->flags |= IFF_NOARP;
    dev->features |= NETIF_F_HW_CSUM;

    struct netdevice_private* priv = netdev_priv(dev);
    memset(priv, 0, sizeof(struct netdevice_private));
    netif_napi_add(dev, &priv->napi, netdevice_poll, 2);
    spin_lock_init(&priv->lock);
    priv->dev = dev;

    netdevice_set_receive_ints(dev, 1);
    netdevice_setup_pool(dev);
}

static int netdevice_init(void)
{
    printk(KERN_INFO "NetDriver: Init\n");

    netdevs[0] = alloc_netdev(sizeof(struct netdevice_private), "netdev%d", NET_NAME_UNKNOWN, netdev_init);
    netdevs[1] = alloc_netdev(sizeof(struct netdevice_private), "netdev%d", NET_NAME_UNKNOWN, netdev_init);

    if (!netdevs[0] || !netdevs[1])
    {
        printk(KERN_INFO "NetDriver: Allocation error\n");
        return -ENODEV;
    }

    if (register_netdev(netdevs[0]) || register_netdev(netdevs[1]))
    {
        printk(KERN_INFO "NetDriver: Registration error\n");
        return -ENODEV;
    }

    return 0;
}

static void netdevice_exit(void)
{
    printk(KERN_INFO "NetDriver: Exit\n");

    int i;
    for (i = 0; i < 2; i++)
    {
        if (netdevs[i])
        {
            unregister_netdev(netdevs[i]);
            netdevice_cleanup_pool(netdevs[i]);
            free_netdev(netdevs[i]);
        }
    }
}

module_init(netdevice_init);
module_exit(netdevice_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("A B");
MODULE_DESCRIPTION("Network driver");
