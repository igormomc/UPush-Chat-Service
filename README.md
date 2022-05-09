# Hjemme eksamen IN2140: UPush Chat Service

### Introduksjon
I denne Readme filen skal gå gjennom hver fil og forklare hvordan det gikk, hva som funker og hva som ikke funger. Jeg skal også komme med insspil og antagelser som kan være til hjelp for å forstå koden/programmet mitt enda bedre. Om jeg har laget noen ektra funksjoner, filer eller gjort noe som er litt uvnalig så nevner jeg det her slik at dere som retter kan enklere skjønne det.<br />
I hver av seksjonene vil jeg Gå gjennom punktvis hvilke valg jeg har tatt og hvorfor jeg har tatt det valget(Følger punktvis deres Anbefalte Utviklingstrinn) 

## Server
* Les parametere fra kommandolinjen og sett tapssannsynlighet ved å kalle
set_loss_probability()
  * Etter å ha lest "Man Pages" Så fant jeg ut at man skal kalle på srand48() funksjonen før man kaller på drand48() funksjonen. Derfor kaller jeg på srand48() funksjonen rett før jeg kaller på set_loss_probability(), slik at den skal bli satt korrekt. 
<br />

* Lag datastrukturer for registrerte klienter
  * Lenket liste som holder på dataen til clientene. Jeg valgte å bare bruke en "en veis" lenkeliste og ikke "to veis" fordi jeg antar at det ikke skal være så mange på denne chatt systemet og trenger ikke at den skal være lynrask.

<br />

* Lag en socket og prøv å sende registrerings- og oppslagsmeldinger ved hjelp av Netcat
(bare for å se at mottaket fungerer)
  * brukte ikke ntoi, og ipv4 her. Dette er bevist slik at jeg kan bruke både IPv4 og IPv6 adresser. 

<br />

* Implementer registrering. Legg til informasjon i datastrukturen som svar på
registreringsmeldinger.
  * Registrering fungerer som den skal. Skriver man i formatet: "PKT nummer REG nick" så registrer man Nick i serveren.


<br />

* Implementer oppslag. Svar på oppslag ved hjelp av datastrukturen og informasjonen i
forespørselen.
  * funker bra


<br />

* Test den ved hjelp av print-statements og Netcat for å sende registrerings- og
oppslagsmeldinger.
  * Kjørte Serveren og fikk null valgrins feil. Connected Serveren med Netcat (nc -u 127.0.0.1 8080). Alt fungerte akkurat som det skulle på SSH-IFI-maskinene. På mac så er det en liten "bug" på grunn av hvordan mac fungerer. Dette er noe som ikke har noe med koden å gjøre og har fått beskjed å bare overse dette fordi det funker bra på Linux. Men jeg kan vise eksempel under på hva jeg mener, hentet fra Mac terminalen:
  ```LOOKUP test
      PKT 1 REG nick
      ACK 1 OK
      PKT 1 LOOKUP nick
      ACK 1 NICK nick
      ::ffff:127.0.0.1 PORT 51905
  ```

## Client
* Les parametere fra kommandolinjen og sett tapssannsynlighet ved å kalle
set_loss_probability().
  * Som nevnt ovenfor i Servern, så gjelder det samme her angående set_loss_probability, Da jeg ikke kalte på srand48() fikk jeg feil probability og derfor viktig å kalle på denne før set_loss_probability(), da funket alt som det skulle.

* Opprett en socket og prøv å sende noe til serveren.
  * Jeg har laget Dette slikt at det skal fungere med både IPv4 og IPv6.
  * På SSH-Ifi maskinene så fungerer det helt fint å sende meldinger fra client til Server(Med melding mener jeg Ack, Looksups, Reg osv..). Men på Mac maskinen min så ser jeg at meldingene ikke vises på terminalen. Ingenting krasjer men får ikke sett meldingene. Etter å ha lest "Man Pages" og snakket med noen gruppelærere har jeg kommet fram til at dette er et mac problem som kanskje handler om at PF_inet og AF_inet ikke fungerer helt likt på mac og linux. Men jeg ville ikke bruke for mye tid på dette da det fungerer fint på ifi sine maskiner som er det viktigste.

* Implementer registrering.
  * Registrering fungerer fint. Når en client registerer seg så blir det sendt en Registreings melding til serveren i formatet: "PKT nummer REG nick" og den blir deretter lagt til i lenkelisten. Meldingen blir laget ved hjelp av funksjonen sprintf() deretter sender vi meldingen men send_packet()

* 



## Timeout

## Heartbeat
* Implementer på serveren og test med Netcat
  * Flott

## Blocking
