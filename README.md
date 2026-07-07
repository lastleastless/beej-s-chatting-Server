
beej's poll chat server improvement

Epoll -Edge Trigger를 이용한 pthread worker pool c socket multithreading server


# Tech Stack:
	Language: C (C11 standard)
	OS: linux
	API: POSIX thread(pthread), linux EPOLL(non-blocking I/O)


# System architecture:
	CPU 코어를 최대한 활용하기 위한 Producer - Consumer pattern 채택.

	1.Main thread.
		epoll_wait loop를 통해 epoll_in event를 Edge Trigger방식으로 감지합니다.
		client가 보낸 data는 TCP를 이용해 수신하므로 데이터가 쪼개지는것을 대비하여 recvall()을 통하여 안전하게 받습니다.
		수신이 완료된 패킷은 queue에 push하고 pthread_cond_signal()을 통하여 워커 플에게 데이터가 들어왔음을 알립니다.
	2.Worker thread pool
	스레드 풀을 만들어 컨텍스트 스위칭을 최소화하고 메인루프와 독립적으로 client들에게 메세지를 브로드캐스팅합니다.

# tech issue:
	처음 소비자 스레드를 구현할 때 깊은 복사를 이용하지 않고, 얕은 복사를 이용하여 데이터 구조체를 참조하였는데,
	이때 생산자 스레드가 패킷을 큐에 밀어 넣으면 값이 덮어씌워지는 race condition이 발생하였습니다.
	이를 해결하기위하여 소비자 스레드에서 데이터 구조체를 참조할 때 깊은 복사를 통해 race condition이 일어나지 않도록
	코드를 정리하였습니다.
