#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/in6.h>
#include <asm/checksum.h>

MODULE_LICENSE("GPL");

#define DEBUG
#ifdef DEBUG
#define WDEBUG(fmt, args...) printk(KERN_INFO "woab: " fmt, ##args)
#define WTRACE() printk(KERN_INFO "woab: %s %d\n", __FUNCTION__, __LINE__)
#else
#define WDEBUG(fmt, args...)
#define WTRACE()
#endif

#define WOAB_RX 0x1
#define WOAB_TX 0x2

#define TRUE 1
#define FALSE 0

#define TIMEOUT 10
#define POOL_SIZE 8

#define MAX_DEVICES 2

struct net_device *woab_devices[MAX_DEVICES];

struct woab_packet {
        struct woab_packet *next;
        struct net_device *dev;
        int datalen;
        u8 data[ETH_DATA_LEN];
};

struct woab_priv {
        struct net_device_stats stats;
        struct net_device *dev;
        struct napi_struct napi;
        struct sk_buff *skb;
        int status;
        struct woab_packet *ppool;
        struct woab_packet *rx_queue;
        int rx_int_enabled;
        int tx_packetlen;
        u8 *tx_packetdata;

        spinlock_t lock;
};

// basic operations
static int woab_open(struct net_device *dev);
static int woab_release(struct net_device *dev);
static int woab_tx(struct sk_buff *skb, struct net_device *dev);
static void woab_tx_timeout(struct net_device *dev, unsigned int txqueue);
static struct net_device_stats *woab_get_stats(struct net_device *dev);

static const struct net_device_ops woab_netdev_ops = {
        .ndo_open = woab_open,
        .ndo_stop = woab_release,
        .ndo_start_xmit = woab_tx,
        .ndo_get_stats = woab_get_stats,
        .ndo_tx_timeout = woab_tx_timeout,
};

// header stuff
static int woab_prepare_header(struct sk_buff *skb, struct net_device *dev,
                               unsigned short type, const void *daddr,
                               const void *saddr, unsigned len);

static const struct header_ops woab_header_ops = {
        .create = woab_prepare_header,
};

// helpers
static void woab_setup_pool(struct net_device *dev);
static void woab_cleanup_pool(struct net_device *dev);
static struct woab_packet *woab_get_tx_buffer(struct net_device *dev);
static void woab_release_buffer(struct woab_packet *pkt);
static void woab_enqueue_buf(struct net_device *dev, struct woab_packet *pkt);
static struct woab_packet *woab_dequeue_buf(struct net_device *dev);
static void woab_set_rx_ints(struct net_device *dev, int enable);
static void woab_napi_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void woab_hw_tx(char *buf, int len, struct net_device *dev);
static void woab_init(struct net_device *dev);

// definitions
static void woab_setup_pool(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);
        int i;
        struct woab_packet *pkt;

        WTRACE();

        priv->ppool = NULL;
        for (i = 0; i < POOL_SIZE; i++) {
                pkt = kmalloc(sizeof(struct woab_packet), GFP_KERNEL);
                if (pkt == NULL) {
                        WDEBUG("OOM allocating packet pool\n");
                        return;
                }
                pkt->dev = dev;
                pkt->next = priv->ppool;
                priv->ppool = pkt;
        }
}

static void woab_cleanup_pool(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);
        struct woab_packet *pkt;

        WTRACE();

        while ((pkt = priv->ppool)) {
                priv->ppool = pkt->next;
                kfree(pkt);
        }
}

static struct woab_packet *woab_get_tx_buffer(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);
        unsigned long flags;
        struct woab_packet *pkt;

        WTRACE();

        spin_lock_irqsave(&priv->lock, flags);
        pkt = priv->ppool;
        if (!pkt) {
                WDEBUG("Out of Pool\n");
                return pkt;
        }
        priv->ppool = pkt->next;
        if (priv->ppool == NULL) {
                WDEBUG("Pool is empty\n");
                netif_stop_queue(dev);
        }
        spin_unlock_irqrestore(&priv->lock, flags);
        return pkt;
}

static void woab_release_buffer(struct woab_packet *pkt)
{
        unsigned long flags;
        struct woab_priv *priv = netdev_priv(pkt->dev);

        WTRACE();

        spin_lock_irqsave(&priv->lock, flags);
        pkt->next = priv->ppool;
        priv->ppool = pkt;
        spin_unlock_irqrestore(&priv->lock, flags);
        if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
                netif_wake_queue(pkt->dev);
}

static void woab_enqueue_buf(struct net_device *dev, struct woab_packet *pkt)
{
        unsigned long flags;
        struct woab_priv *priv = netdev_priv(dev);

        WTRACE();

        spin_lock_irqsave(&priv->lock, flags);
        pkt->next = priv->rx_queue;
        priv->rx_queue = pkt;
        spin_unlock_irqrestore(&priv->lock, flags);
}

static struct woab_packet *woab_dequeue_buf(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);
        struct woab_packet *pkt;
        unsigned long flags;

        WTRACE();

        spin_lock_irqsave(&priv->lock, flags);
        pkt = priv->rx_queue;
        if (pkt != NULL)
                priv->rx_queue = pkt->next;
        spin_unlock_irqrestore(&priv->lock, flags);
        return pkt;
}

static void woab_set_rx_ints(struct net_device *dev, int enable)
{
        struct woab_priv *priv = netdev_priv(dev);
        WTRACE();
        priv->rx_int_enabled = enable;
}

static int woab_open(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);
        int i;
        u8 mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0x00 };

        WTRACE();

        memcpy(dev->dev_addr, mac, ETH_ALEN);
        for (i = 0; i < MAX_DEVICES; i++)
                if (dev == woab_devices[i])
                        dev->dev_addr[ETH_ALEN - 1] += i;

        napi_enable(&priv->napi);

        netif_start_queue(dev);
        return 0;
}

static int woab_release(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);

        WTRACE();

        netif_stop_queue(dev);

        napi_disable(&priv->napi);
        return 0;
}

static int woab_poll(struct napi_struct *napi, int budget)
{
        int npackets = 0;
        struct sk_buff *skb;
        struct woab_priv *priv = container_of(napi, struct woab_priv, napi);
        struct net_device *dev = priv->dev;
        struct woab_packet *pkt;

        WTRACE();

        while (npackets < budget && priv->rx_queue) {
                pkt = woab_dequeue_buf(dev);
                skb = dev_alloc_skb(pkt->datalen + 2);
                if (!skb) {
                        WDEBUG("can't alloc skb - packet dropped\n");
                        priv->stats.rx_dropped++;
                        npackets++;
                        woab_release_buffer(pkt);
                        continue;
                }
                skb_reserve(skb, 2);
                memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
                skb->dev = dev;
                skb->protocol = eth_type_trans(skb, dev);
                skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
                netif_receive_skb(skb);

                npackets++;
                priv->stats.rx_packets++;
                priv->stats.rx_bytes += pkt->datalen;
                woab_release_buffer(pkt);
        }

        if (npackets < budget) {
                unsigned long flags;
                spin_lock_irqsave(&priv->lock, flags);
                if (napi_complete_done(napi, npackets))
                        woab_set_rx_ints(dev, TRUE);
                spin_unlock_irqrestore(&priv->lock, flags);
        }

        return npackets;
}

static void woab_napi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
        int status;
        struct woab_priv *priv;
        struct net_device *dev = (struct net_device *)dev_id;

        WTRACE();

        if (!dev)
                return;

        priv = netdev_priv(dev);
        spin_lock(&priv->lock);

        status = priv->status;
        priv->status = 0;
        if (status & WOAB_RX) {
                woab_set_rx_ints(dev, FALSE);
                napi_schedule(&priv->napi);
        }
        if (status & WOAB_TX) {
                priv->stats.tx_packets++;
                priv->stats.tx_bytes += priv->tx_packetlen;
                if (priv->skb) {
                        dev_kfree_skb(priv->skb);
                        priv->skb = 0;
                }
        }

        spin_unlock(&priv->lock);

        return;
}

static void woab_hw_tx(char *buf, int len, struct net_device *dev)
{
        struct iphdr *ih;
        struct net_device *dest;
        struct woab_priv *priv;
        u32 *saddr, *daddr;
        struct woab_packet *tx_buffer;

        WTRACE();

        if (len < sizeof(struct ethhdr) + sizeof(struct iphdr)) {
                WDEBUG("Error: packet too short (%i octets)\n", len);
                return;
        }

        ih = (struct iphdr *)(buf + sizeof(struct ethhdr));
        saddr = &ih->saddr;
        daddr = &ih->daddr;

        if (dev == woab_devices[0])
                WDEBUG("orig: %08x:%05i --> %08x:%05i\n", ntohl(ih->saddr),
                       ntohs(((struct tcphdr *)(ih + 1))->source),
                       ntohl(ih->daddr),
                       ntohs(((struct tcphdr *)(ih + 1))->dest));
        else
                WDEBUG("orig: %08x:%05i <-- %08x:%05i\n", ntohl(ih->daddr),
                       ntohs(((struct tcphdr *)(ih + 1))->dest),
                       ntohl(ih->saddr),
                       ntohs(((struct tcphdr *)(ih + 1))->source));

        // do magic here to change networks
        ((u8 *)saddr)[2] ^= 1;
        ((u8 *)daddr)[2] ^= 1;

        // fix checksum after IP changes
        ih->check = 0;
        ih->check = ip_fast_csum((unsigned char *)ih, ih->ihl);

        if (dev == woab_devices[0])
                WDEBUG("%08x:%05i --> %08x:%05i\n", ntohl(ih->saddr),
                       ntohs(((struct tcphdr *)(ih + 1))->source),
                       ntohl(ih->daddr),
                       ntohs(((struct tcphdr *)(ih + 1))->dest));
        else
                WDEBUG("%08x:%05i <-- %08x:%05i\n", ntohl(ih->daddr),
                       ntohs(((struct tcphdr *)(ih + 1))->dest),
                       ntohl(ih->saddr),
                       ntohs(((struct tcphdr *)(ih + 1))->source));

        dest = woab_devices[dev == woab_devices[0] ? 1 : 0];
        priv = netdev_priv(dest);
        tx_buffer = woab_get_tx_buffer(dev);

        if (!tx_buffer) {
                WDEBUG("Out of tx buffer, len is %i\n", len);
                return;
        }

        tx_buffer->datalen = len;
        memcpy(tx_buffer->data, buf, len);
        woab_enqueue_buf(dest, tx_buffer);
        if (priv->rx_int_enabled) {
                priv->status |= WOAB_RX;
                woab_napi_interrupt(0, dest, NULL);
        }

        priv = netdev_priv(dev);
        priv->tx_packetlen = len;
        priv->tx_packetdata = buf;
        priv->status |= WOAB_TX;

        woab_napi_interrupt(0, dev, NULL);
}

static int woab_tx(struct sk_buff *skb, struct net_device *dev)
{
        int len;
        char *data, shortpkt[ETH_ZLEN];
        struct woab_priv *priv = netdev_priv(dev);

        WTRACE();

        data = skb->data;
        len = skb->len;
        if (len < ETH_ZLEN) {
                memset(shortpkt, 0, ETH_ZLEN);
                memcpy(shortpkt, skb->data, skb->len);
                len = ETH_ZLEN;
                data = shortpkt;
        }
        netif_trans_update(dev);

        priv->skb = skb;

        woab_hw_tx(data, len, dev);

        return 0;
}

static void woab_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
        struct woab_priv *priv = netdev_priv(dev);
#ifdef DEBUG
        struct netdev_queue *txq = netdev_get_tx_queue(dev, 0);
#endif
        WTRACE();

        WDEBUG("Transmit timeout at %ld, latency %ld\n", jiffies,
               jiffies - txq->trans_start);

        priv->status |= WOAB_TX;
        woab_napi_interrupt(0, dev, NULL);
        priv->stats.tx_errors++;

        spin_lock(&priv->lock);
        woab_cleanup_pool(dev);
        woab_setup_pool(dev);
        spin_unlock(&priv->lock);

        netif_wake_queue(dev);
        return;
}

static struct net_device_stats *woab_get_stats(struct net_device *dev)
{
        struct woab_priv *priv = netdev_priv(dev);
        WTRACE();
        return &priv->stats;
}

static int woab_prepare_header(struct sk_buff *skb, struct net_device *dev,
                               unsigned short type, const void *daddr,
                               const void *saddr, unsigned len)
{
        struct ethhdr *eth = (struct ethhdr *)skb_push(skb, ETH_HLEN);

        WTRACE();

        eth->h_proto = htons(type);
        memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
        memcpy(eth->h_dest, daddr ? daddr : dev->dev_addr, dev->addr_len);
        eth->h_dest[ETH_ALEN - 1] ^= 0x01;
        WDEBUG("src mac = %02x:%02x:%02x:%02x:%02x:%02x\n", dev->dev_addr[0],
               dev->dev_addr[1], dev->dev_addr[2], dev->dev_addr[3],
               dev->dev_addr[4], dev->dev_addr[5]);
        WDEBUG("dst mac = %02x:%02x:%02x:%02x:%02x:%02x\n", eth->h_dest[0],
               eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4],
               eth->h_dest[5]);
        return (dev->hard_header_len);
}

static void woab_init(struct net_device *dev)
{
        struct woab_priv *priv;

        WTRACE();

        ether_setup(dev);
        dev->watchdog_timeo = TIMEOUT;
        dev->netdev_ops = &woab_netdev_ops;
        dev->header_ops = &woab_header_ops;

        dev->flags |= IFF_NOARP;
        dev->features |= NETIF_F_HW_CSUM;

        priv = netdev_priv(dev);
        memset(priv, 0, sizeof(struct woab_priv));

        netif_napi_add(dev, &priv->napi, woab_poll, 2);

        spin_lock_init(&priv->lock);
        priv->dev = dev;

        woab_set_rx_ints(dev, TRUE);
        woab_setup_pool(dev);
}

static void woab_exit_module(void)
{
        int i;
        WTRACE();
        for (i = 0; i < MAX_DEVICES; i++) {
                if (woab_devices[i]) {
                        unregister_netdev(woab_devices[i]);
                        woab_cleanup_pool(woab_devices[i]);
                        free_netdev(woab_devices[i]);
                }
        }
        return;
}

static int woab_init_module(void)
{
        int result, i, ret = -ENOMEM;

        WTRACE();
        for (i = 0; i < MAX_DEVICES; i++) {
                woab_devices[i] =
                        alloc_netdev(sizeof(struct woab_priv), "woab%d",
                                     NET_NAME_UNKNOWN, woab_init);
                if (woab_devices[i] == NULL)
                        goto out;
        }

        ret = -ENODEV;
        for (i = 0; i < MAX_DEVICES; i++)
                if ((result = register_netdev(woab_devices[i])))
                        WDEBUG("error %d registering device \"%s\"\n", result,
                               woab_devices[i]->name);
                else
                        ret = 0;
out:
        if (ret)
                woab_exit_module();
        return ret;
}

module_init(woab_init_module);
module_exit(woab_exit_module);