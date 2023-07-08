
#include <rte_byteorder.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <rte_flow.h>

/* set specific source port to specific queue */
void
flow_insert ( uint16_t port_id, uint16_t rxq, uint16_t udp_src_port )
{
  struct rte_flow_item_udp udp = { .hdr.src_port = udp_src_port };
  struct rte_flow_item_udp udp_mask = { .hdr.src_port = 0xffff };

  struct rte_flow_item pattern[] = {
    { .type = RTE_FLOW_ITEM_TYPE_ETH },  /* pass all eth packets */
    { .type = RTE_FLOW_ITEM_TYPE_IPV4 }, /* pass all ipv4 packets */

    /* match source port udp */
    { .type = RTE_FLOW_ITEM_TYPE_UDP, .spec = &udp, .mask = &udp_mask },
    { .type = RTE_FLOW_ITEM_TYPE_END } /* end pattern match */
  };

  /* queue to send packet */
  struct rte_flow_action_queue queue = { .index = rxq };

  /* create the queue action */
  struct rte_flow_action action[] = { { .type = RTE_FLOW_ACTION_TYPE_QUEUE,
                                        .conf = &queue },
                                      { .type = RTE_FLOW_ACTION_TYPE_END } };

  /* validate and create the flow rule */
  struct rte_flow_attr attr = { .ingress = 1 }; /* match only ingress traffic */
  struct rte_flow_error error;

  if ( rte_flow_validate ( port_id, &attr, pattern, action, &error ) )
    {
      fprintf ( stderr, "Error rte_flow_validate: %s\n", error.message );
      exit ( EXIT_FAILURE );
    }

  if ( !rte_flow_create ( port_id, &attr, pattern, action, &error ) )
    {
      fprintf ( stderr, "Error rte_flow_create: %s\n", error.message );
      exit ( EXIT_FAILURE );
    }
}
