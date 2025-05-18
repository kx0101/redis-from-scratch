# redis protocol parser

## SET
➜ echo -e '*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n' | nc 127.0.0.1 6379
<br />
+OK

## GET
➜ echo -e '*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n' | nc 127.0.0.1 6379
<br />
$3
bar
