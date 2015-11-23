# These following 4 aliases are to prevent an unwanted shutdown 
# instead of a reboot when developing from a remote location.
# There is no guarantee to prevent the user to escape the rules!

alias sudo='sudo '
#alias shutdown='echo "not allowed"'
alias poweroff='echo "not allowed"'
alias halt='echo "not allowed"'

alias helpcommands="cat /home/pi/helpcommands"
alias ll='ls -lh'
alias l.='ls -lah'
alias extip="curl http://ipecho.net/plain; echo"
alias c='highlight -O ansi -l -t 4'
alias tail='tail -n30'
