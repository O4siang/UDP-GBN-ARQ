all: sender receiver sender_crc receiver_crc

sender: sender.o
	g++ sender.o -o sender

receiver: receiver.o
	g++ receiver.o -o receiver

sender_crc: sender_crc.cpp
	g++ -o sender_crc sender_crc.cpp -lz

receiver_crc: receiver_crc.cpp
	g++ -o receiver_crc receiver_crc.cpp -lz

sender.o: sender.cpp
	g++ -c sender.cpp

receiver.o: receiver.cpp
	g++ -c receiver.cpp

clean:
	rm -rf *o sender receiver sender_crc receiver_crc