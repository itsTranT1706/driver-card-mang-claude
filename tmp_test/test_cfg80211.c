#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>

static int __init my_init(void)
{
    struct wiphy *wiphy;
    struct cfg80211_ops ops = {};
    wiphy = wiphy_new(&ops, 0);
    return wiphy ? 0 : -ENOMEM;
}
static void __exit my_exit(void) {}
module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
