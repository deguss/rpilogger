ads: main_2.c save_2.c i2c.c ads1115.c
	gcc -o ads main_2.c -lrt -lnetcdf -lm -lpthread -Wall
deb: main_2.c
	gcc -g -o ads main_2.c -lrt -lnetcdf -lpq -lm -lpthread -lnopoll -I/usr/include/nopoll -Wall
