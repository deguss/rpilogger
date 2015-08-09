// gcc -o hrt hrtimer.c -lrt

#include <linux/hrtimer.h>
#include <linux/sched.h>

//-------------------------------------------------------------
// Timer variables block
static enum hrtimer_restart function_timer(struct hrtimer *);
static struct hrtimer htimer;
static ktime_t kt_periode;


//-------------------------------------------------------------
static enum hrtimer_restart function_timer(struct hrtimer * unused){
//-------------------------------------------------------------
    if (gpio_current_state==0){
        gpio_set_value(GPIO_OUTPUT,1);
        gpio_current_state=1;
    }
    else{
        gpio_set_value(GPIO_OUTPUT,0);
        gpio_current_state=0;
    }
    hrtimer_forward_now(& htimer, kt_periode);
    return HRTIMER_RESTART;
}


//=============================================================
int main(int argc, char * argv[]){
//=============================================================
	kt_periode = ktime_set(0, 100); //seconds,nanoseconds
	hrtimer_init (& htimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	htimer.function = function_timer;
	hrtimer_start(& htimer, kt_periode, HRTIMER_MODE_REL);

	return 0;
}
