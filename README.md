
beej's poll chat server improvement suggestion

1. modulize all subfunctions.
2. add graceful exit
3. store all IP address of remoteIp
4. add ID feature
5. add whisper feature
6. serialzation / deserialization
7. ensure all bytes of packet are received/sended.


2 -> fatal error is occured. exit(1) -> send close() to all client and log error, then exit(1)

5 -> using bloomfilter to find ID at O(1) 

4 -> add ID feature. Done

6 -> pure string message -> serialize size + Id + data packet

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

7-> sendall -> done
    recvall -> under construction.
