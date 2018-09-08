# UDP-GBN-ARQ
Compile the program:
> make

Output will be 4 files sender, receiver, sender_crc, receiver_crc


(1)Exec the program:  
In XTerm receiver  
> ./receiver  
> 
In XTerm sender  
> ./sender

(2)Description:
1. First run ./receiver, create the UDP socket on assigned port and waiting for receiving. 
2. Run ./sender and input at least length 10 string (without space in the string)
3. program started sending and receiving the packets. working as GBN-ARQ
4. Once sender finished sending data and received the Acked FIN packet. Print "Done" and closed the socket.
5. Once receiver received the FIN packet, printed the completion string, timer starting for 5 seconds and then closed the socket. Executed the new loop and socket for waiting input packets.

（3）Design:

(4)Results and analysis: