all: sender.c agent.c receiver.c sender_b.c receiver_b.c
	gcc sender.c -o sender
	gcc agent.c -o agent
	gcc receiver.c -o receiver
	gcc sender_b.c -o sender_b
	gcc receiver_b.c -o receiver_b

run:
	./sender 3000 3001 pusheens.jpg > sender.log &
	./agent 3001 3000 3002 3 > agent.log &
	./receiver 3002 3001 pusheens2.jpg > receiver.log
	wait

run_b:
	./sender_b 3000 3001 3002 3003 pusheens.jpg > sender_b.log &
	./agent 3001 3000 3004 2 > agent1.log &
	./agent 3002 3000 3004 2 > agent2.log &
	./agent 3003 3000 3004 2 > agent3.log &
	./receiver_b 3004 3001 3002 3003 pusheens2.jpg > receiver_b.log
	wait

clean:
	rm -f sender agent receiver sender_b receiver_b
