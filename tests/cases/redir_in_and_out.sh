echo first > redir_both.txt
cat < redir_both.txt > redir_both2.txt
cat redir_both2.txt
rm redir_both.txt redir_both2.txt