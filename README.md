
beej's poll chat server improvement suggestion

1. modulize all subfunctions.
2. add graceful exit
3. add ID feature
4. serialzation / deserialization
5. ensure all bytes of packet are received/sended.
6. migrate poll() to epoll()
7. add Multi-Threading server model (Producer / Consumer model) 

2 -> fatal error is occured. exit(1) -> send close() to all client and log error, then exit(1) 

3 -> add ID feature. Done

4 -> pure string message -> serialize size + Id + data packet

  -> make sendall / recvall function to ensure robust data streaming
  
  -> struct userinfo
	char id[11];
	char buf[101];

  -> data packet
	->   size  IDSIZE ID     dataSIZE  data
	     2byte 2byte  10byte 2byte     100byte
	-> ID, data -> No null terminator
	-> receiver must add '\0' to get c-style string
  -> doesn't have protection to overflow input.
  -> add overlflow protectino both server side and client size.

5-> sendall -> done
    recvall -> done

6-> server -> producer -> get packet from client and put packet in buffer
           -> consumer  -> get packet from buffer and give packet to sender.
           -> semaphore value: fill = 0, empty = sizeof buffer, m = 1
           -> or using pthread_mutex_lock, unlock, pthread_cond_wait, pthread_cond_signal ..
           -> thread creation -> when accept comes to server -> make consumer thread for specific client.
                              -> create producer thread when server is start.
           -> under construction...

