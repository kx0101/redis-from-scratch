# redis protocol parser

➜  echo -e '*2\r\n$4\r\nECHO\r\n$3\r\nhey\r\n' | nc 127.0.0.1 6379
hey
