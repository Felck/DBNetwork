# DBNetwork

Netzwerkinterface für Datenbanksysteme im Rahmen einer Master-Projektarbeit.

# Server

Verwendet epoll aus dem Linux-Kernel für O(1) I/O-Multiplexing.

# Client

Startet mehrere Threads, die nach einem Poissonprozess modelliert zufällig Pakete an den Server senden und die antworten validieren.
