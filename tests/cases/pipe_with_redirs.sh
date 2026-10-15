echo hi | cat > pipe_redir_out.txt
cat pipe_redir_out.txt
rm pipe_redir_out.txt
printf "b\na\n" > pipe_redir_in.txt
cat < pipe_redir_in.txt | sort
rm pipe_redir_in.txt