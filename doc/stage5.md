# SO-shell-etap5
*Specyfikacja piątego etapu projektu realizowanego w ramach ćwiczeń do przedmiotu Systemy Operacyjne.*

## Implementacja shella - etap 5 (wykonywanie procesów w tle i obsługa sygnałów).

1. Wszystkie polecenia linii w której ostatnim niebiałym znakiem jest `&` powinny być wykonane w tle. Proces shella powinien je uruchomić ale nie czekać aż się zakończą. Można założyć, że jeśli polecenia w pipeline mają być uruchomione w tle to wśród nich  nie występują komendy shella (tzn. wszystkie polecenia można potraktować jak programy). Do sprawdzenia czy pipeline ma być wykonany w tle służy flaga `LINBACKGROUND` w polu `flags` struktury `pipeline`.

2. Proces shella powinien sprawdzać statusy zakończonych dzieci. W przypadku pracy bez wypisywania prompta statusy te można ignorować, w przypadku trybu z promptem należy wypisywać informacje o zakończonych procesach z tła **przed kolejnym  wypisaniem prompta** w formacie:
```
Background process (pid) terminated. (exited with status (status))
Background process (pid) terminated. (killed by signal (signo))
```

3. Należy zadbać o to żeby nie zostawiać procesów zombie. W przypadku trybu z promptem należy uwzględnić przypadek kiedy prompt nie będzie wypisany przez długi czas (np. `sleep 10 | sleep 10 & sleep 1000000`). Dopuszaczalne jest rozwiązanie które przechowuje jedynie ustaloną liczbę statusów (będącą stałą kompilacji) zakończeń dzieci aż do wypisania prompta a statusy pozostałych dzieci zapomina.

4. CTRL-C wysyła sygnał `SIGINT` do wszystkich procesów z grupy procesów foreground'u. Należy zadbać o to żeby sygnał ten nie docierał do procesów z tła. Przy tym procesy uruchomione w tle powinny mieć zarejestrowaną domyślną obsługę tego sygnału.

Syscall checklist: `sigaction`, `sigprocmask`, `sigsuspend`, `setsid`


Przykład sesji:
```
$ sleep 20 | sleep 21 | sleep 22 &
$ ps ax | grep sleep
  580   ?  0:00 sleep TERM=xterm
  581   ?  0:00 sleep TERM=xterm
  582   ?  0:00 sleep TERM=xterm
  586  p1  0:00 grep sleep
$
Background process 580 terminated. (exited with status 0)
Background process 581 terminated. (exited with status 0)
Background process 582 terminated. (exited with status 0)
$ sleep 10 | sleep 10 | sleep 10 &
$ sleep 30	# (ten sleep nie powinien być przerwany wcześniej niż po 30s)
Background process 587 terminated. (exited with status 0)
Background process 588 terminated. (exited with status 0)
Background process 589 terminated. (exited with status 0)
$ sleep 10 | sleep 10 | sleep 10 &
$ sleep 1000 # (z innego terminala sprawdź czy nie ma procesów zombie po przynajmniej 10s ale przed zakończeniem sleep 1000)
(CTRL-C) (powinien przerwać sleep 1000)
Background process 591 terminated. (exited with status 0)
Background process 592 terminated. (exited with status 0)
Background process 593 terminated. (exited with status 0)
$ sleep 1000 &
$ sleep 10
(CTRL-C) (powinien przerwać sleep 10 ale nie sleep 1000)
$ ps ax | grep sleep
595   ?  0:00 sleep TERM=xterm
598  p1  0:00 grep sleep
$ /bin/kill -SIGINT 595
Background process 595 terminated. (killed by signal 2)
$ exit
```
