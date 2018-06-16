#include <uk/config.h>
#include <lwip/tcpip.h>
#include <lwip/init.h>
#include <uk/plat/ctors.h>
#include <lwip-netif.h>

/* This function is called before the any other sys_arch-function is
 * called and is meant to be used to initialize anything that has to
 * be up and running for the rest of the functions to work. for
 * example to set up a pool of semaphores. */
void sys_init(void)
{
    return;
}

/*
 * This function initializing the lwip network stack
 *
 */

#if 0
int liblwip_init(void)
{
#if CONFIG_LIBUKSCHED
        tcpip_init(NULL, NULL);
#else
        lwip_init();
#endif /* CONFIG_LIBUKSCHED */
	return 0;
}
#endif

int liblwip_init(void)
{
	start_networking();
	/* TODO add some error handling */
	return 0;
}

int liblwip_init2(void)
{
	start_networking2();
	/* TODO add some error handling */
	return 0;
}
