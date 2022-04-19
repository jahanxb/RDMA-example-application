#include<stdlib.h>
#include<stdint.h>
#include<infiniband/arch.h>
#include<rdma/rdma_cma.h>
#include<string.h>

enum {
RESOLVE_TIMEOUT_MS = 5000,
};

struct pdata {
	char buf_va;
	char buf_rkey;

};

int main(int argc, char *argv[]){

    char temp[10];
    struct pdata rep_pdata;
    struct rdma_event_channel *cm_channel;
    struct rdma_cm_id *listen_id;

    struct rdma_cm_id           *cm_id; 
    struct rdma_cm_event        *event; 
    struct rdma_conn_param      conn_param = { };

    struct ibv_pd           *pd; 
    struct ibv_comp_channel *comp_chan; 
    struct ibv_cq           *cq;  
    struct ibv_cq           *evt_cq;
    struct ibv_mr           *mr; 
    struct ibv_qp_init_attr qp_attr = { };
    struct ibv_sge          sge; 
    struct ibv_send_wr      send_wr = { };
    struct ibv_send_wr      *bad_send_wr; 
    struct ibv_recv_wr      recv_wr = { };
    struct ibv_recv_wr      *bad_recv_wr;
    struct ibv_wc           wc;

    struct sockaddr_in sin;    


    void *cq_context;
    
    char *buf;
    
    int err;

// setup RDMA CM structures

cm_channel = rdma_create_event_channel();

if (!cm_channel){
return 1;
}

err = rdma_create_id(cm_channel,&listen_id,NULL,RDMA_PS_TCP);

if (err){
return err;
}

sin.sin_family = AF_INET;
sin.sin_port = htons(20079);
sin.sin_addr.s_addr = INADDR_ANY;


// Bind to local port and listen for connection request 

err = rdma_bind_addr(listen_id,(struct sockaddr *) &sin);

if(err){
return 1;
}

err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
        return err;

}

    if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST)
{
        return 1;
}

    cm_id = event->id;

    rdma_ack_cm_event(event);


// create verbs objects now that we know which device to use

pd = ibv_alloc_pd(cm_id->verbs);

if(!pd){
return 1;
}

comp_chan = ibv_create_comp_channel(cm_id->verbs);

if(!comp_chan){
return 1;
}


cq = ibv_create_cq(cm_id->verbs, 2, NULL, comp_chan, 0); 
    if (!cq){
        return 1;
}
    if (ibv_req_notify_cq(cq, 0)){
        return 1;

 }

buf = calloc(2,sizeof(char));

if (!buf) {
return 1;
}

mr = ibv_reg_mr(pd,buf,2 * sizeof(char),

	IBV_ACCESS_LOCAL_WRITE |
	IBV_ACCESS_REMOTE_READ |
	IBV_ACCESS_REMOTE_WRITE);

if(!mr){
return 1;

}

qp_attr.cap.max_send_wr = 1;
qp_attr.cap.max_send_sge = 1;
qp_attr.cap.max_recv_wr = 1;
qp_attr.cap.max_recv_sge = 1;

qp_attr.send_cq = cq;
qp_attr.recv_cq = cq;

qp_attr.qp_type = IBV_QPT_RC;


err = rdma_create_qp(cm_id,pd,&qp_attr);

if(err){

	return err;
}

// post receive before accepting connection

sge.addr = (char) buf + sizeof(char);
sge.length = sizeof(char);
sge.lkey = mr->lkey;

recv_wr.sg_list = &sge;
recv_wr.num_sge = 1;

if(ibv_post_recv(cm_id->qp,&recv_wr,&bad_recv_wr)){
return 1;

}


rep_pdata.buf_va = htonl((char) buf);
rep_pdata.buf_rkey = htonl(mr->rkey);

conn_param.responder_resources = 1;  
    conn_param.private_data = &rep_pdata; 
    conn_param.private_data_len = sizeof rep_pdata;


// accept connection 

    err = rdma_accept(cm_id, &conn_param); 
    if (err) 
        return 1;

    err = rdma_get_cm_event(cm_channel, &event);
    if (err) 
        return err;

    if (event->event != RDMA_CM_EVENT_ESTABLISHED)
        return 1;

    rdma_ack_cm_event(event);

// wait for receive completion 

if (ibv_get_cq_event(comp_chan, &evt_cq, &cq_context))
        return 1;
    
    if (ibv_req_notify_cq(cq, 0))
        return 1;
    
    if (ibv_poll_cq(cq, 1, &wc) < 1)
        return 1;
    
    if (wc.status != IBV_WC_SUCCESS) //spark error
        return 1;


// previously it was 2 int , now we have two chars 


//temp = strcat(ntohl(buf[0]),ntohl(buf[1]));

strcat(ntohl(buf[0]),ntohl(buf[1]));

//printf("%s",a);


/* -------end of my chawal-------*/

//temp = ntohl(buf[0])


//buf[0] = htonl(ntohl(buf[0])+ntohl(buf[1]));
buf[0] = htonl(buf[0]);

sge.addr = (char) buf;
sge.length = sizeof (char);
sge.lkey = mr->lkey;




send_wr.opcode = IBV_WR_SEND;
send_wr.send_flags = IBV_SEND_SIGNALED;
send_wr.sg_list = &sge;
send_wr.num_sge = 1;

if(ibv_post_send(cm_id->qp,&send_wr,&bad_send_wr)){
	return 1;

}


// wait for send completion 

if (ibv_get_cq_event(comp_chan, &evt_cq, &cq_context)){
return 1;
}


if (ibv_poll_cq(cq, 1, &wc) < 1){ 
        return 1;}

    if (wc.status != IBV_WC_SUCCESS){ 
        return 1;}

    ibv_ack_cq_events(cq,2);
    return 0;

}

