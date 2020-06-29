#/bin/sh
rm -f gen/*.tmp
FILE_LEN=$(wc -c <"gen/index.htm")
echo -e -n "HTTP/1.1 200 OK\r\n">gen/index.tmp
echo -e -n "Content-Type: text/html; charset=utf-8\r\n">>gen/index.tmp
echo -e -n "Content-Length: $FILE_LEN\r\n">>gen/index.tmp
echo -e -n "\r\n">>gen/index.tmp
cat gen/index.htm>>gen/index.tmp
xxd -i gen/index.tmp>gen/index_html.h