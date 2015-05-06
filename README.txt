MIP-daemon kjøres slik:
./mipd [MIP-adresser] [-d for debug]

Eksempel: ./mipd 20 30 40 -d

Rutingdaemon kjøres slik:
./rd [Lokal MIP]

Eksempel: ./rd 20
Obs: Lokal MIP = den første MIP-adressen i listen over MIP-adresser MIP-daemonen er konfigurert med

Filserveren kjøres slik:
./files [Lokal MIP] [Port] [Filnavn]

Eksempel: ./files 20 4050 recv.txt

Filklienten kjøres slik:
./filec [Lokal MIP] [Filnavn] [DestinasjonsMIP] [Port]

Eksempel:
./filec 30 send.txt 20 4050

Obs: Lokal MIP = MIP-adressen rutingdaemonen er konfigurert med