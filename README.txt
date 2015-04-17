MIP-daemon kjøres slik:
./mipd [MIP-adresser] [-d for debug]

Eksempel: ./mipd 20 30 40 -d

Rutingdaemon kjøres slik:
./rd [Lokal MIP]

Eksempel: ./mipd 20
Obs: Lokal MIP = den første MIP-adressen i listen over MIP-adresser MIP-daemonen er konfigurert med

PING-server kjøres slik:
./pings [Lokal MIP]

Eksempel: ./pings 20

PING-klient kjøres slik:
./pingc [Lokal MIP] [DestinasjonsMIP] [Beskjed]

Eksempel: ./pingc 20 100 Hallo