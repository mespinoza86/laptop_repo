#define XTC_TIMER_MAP_SIZE		0X10000


#define XTC_TCSR_OFFSET		0x00000000	/**< Control/Status register */
#define XTC_TLR_OFFSET		0x00000004	/**< Load register */
#define XTC_TCR_OFFSET		0x00000008	/**< Timer counter register */

/* @} */

/** @name Control Status Register Bit Definitions
 * Control Status Register bit masks
 * Used to configure the timer counter device.
 * @{
 */

#define XTC_CSR_CASC_MASK		0x00000800 /**< Cascade Mode */
#define XTC_CSR_ENABLE_ALL_MASK		0x00000400 /**< Enables all timer
							counters */
#define XTC_CSR_ENABLE_PWM_MASK		0x00000200 /**< Enables the Pulse Width
							Modulation */
#define XTC_CSR_INT_OCCURED_MASK	0x00000100 /**< If bit is set, an
							interrupt has occured.
							If set and '1' is
							written to this bit
							position, bit is
							cleared. */
#define XTC_CSR_ENABLE_TMR_MASK		0x00000080 /**< Enables only the
							specific timer */
#define XTC_CSR_ENABLE_INT_MASK		0x00000040 /**< Enables the interrupt
							output. */
#define XTC_CSR_LOAD_MASK		0x00000020 /**< Loads the timer using
							the load value provided
							earlier in the Load
							Register,
							XTC_TLR_OFFSET. */
#define XTC_CSR_AUTO_RELOAD_MASK	0x00000010 /**< In compare mode,
							configures
							the timer counter to
							reload  from the
							Load Register. The
							default  mode
							causes the timer counter
							to hold when the compare
							value is hit. In capture
							mode, configures  the
							timer counter to not
							hold the previous
							capture value if a new
							event occurs. The
							default mode cause the
							timer counter to hold
							the capture value until
							recognized. */
#define XTC_CSR_EXT_CAPTURE_MASK	0x00000008 /**< Enables the
							external input
							to the timer counter. */
#define XTC_CSR_EXT_GENERATE_MASK	0x00000004 /**< Enables the
							external generate output
							for the timer. */
#define XTC_CSR_DOWN_COUNT_MASK		0x00000002 /**< Configures the timer
							counter to count down
							from start value, the
							default is to count
							up.*/
#define XTC_CSR_CAPTURE_MODE_MASK	0x00000001 /**< Enables the timer to
							capture the timer
							counter value when the
							external capture line is
							asserted. The default
							mode is compare mode.*/



int init_timer(uint32_t *ptr){
//	printf("Init timer\n");
	// Load 0 in  load reg
	*(ptr + XTC_TLR_OFFSET/4) = 0x00000003;
	// Move from load to counter reg
	*(ptr + XTC_TCSR_OFFSET/4) = XTC_CSR_INT_OCCURED_MASK | XTC_CSR_LOAD_MASK |  XTC_CSR_ENABLE_TMR_MASK | XTC_CSR_AUTO_RELOAD_MASK | XTC_CSR_ENABLE_INT_MASK;
	// Realese reg
	*(ptr + XTC_TCSR_OFFSET/4) =  0x00000000;
	return 0;
}

int rst_timer(uint32_t *ptr){
	//reset counter
	int tmp;
	uint32_t ctrlreg=0;
	*(ptr + XTC_TLR_OFFSET/4) = 0x00000001;
	ctrlreg = *(ptr + XTC_TCSR_OFFSET/4);
	//printf("reset mask:%x\tTCSR:%x\tTCR:%d\n",XTC_CSR_LOAD_MASK,*(ptr+XTC_TCSR_OFFSET/4),*(ptr+XTC_TCR_OFFSET/4));
	*(ptr + XTC_TCSR_OFFSET/4) = ctrlreg | XTC_CSR_LOAD_MASK;
	tmp = 0;
	while(tmp==0){
		memcpy(&tmp,ptr+(XTC_TCSR_OFFSET),sizeof(int));
//		printf("CTRL %x\n",tmp);
		tmp = (tmp&0x20)>>4;
	}
	//printf("reset mask:%x\tTCSR:%x\tTCR:%d\n",XTC_CSR_LOAD_MASK,*(ptr+XTC_TCSR_OFFSET/4),*(ptr+XTC_TCR_OFFSET/4));
	*(ptr + XTC_TCSR_OFFSET/4) = ctrlreg;
	return 0;
}

int intr_set(uint32_t *ptr){
	
	uint32_t intreg=0;
	intreg = *(ptr + XTC_TCSR_OFFSET/4);
	// Move from load to counter reg
	if((intreg & XTC_CSR_INT_OCCURED_MASK)==XTC_CSR_INT_OCCURED_MASK){
		*(ptr + XTC_TCSR_OFFSET/4) =  intreg | XTC_CSR_INT_OCCURED_MASK;
		rst_timer(ptr);
		printf("Interrupt clear\n");}
	printf("Interrupt ok\n");
	return 0;
}

int set_option(uint32_t *ptr){
//	printf("Init timer\n");

	*(ptr + XTC_TCSR_OFFSET/4) = XTC_CSR_ENABLE_ALL_MASK;
	return 0;
}

int stopT(uint32_t *ptr){
	uint32_t stpreg=0;
	stpreg = *(ptr + XTC_TCSR_OFFSET/4);
	stpreg &= ~(XTC_CSR_ENABLE_TMR_MASK); 
	//stpreg =  0x00000000;
	*(ptr + XTC_TCSR_OFFSET/4) = stpreg;
	return 0;
}


uint32_t get_timer(uint32_t *ptr){
	return *(ptr + XTC_TCR_OFFSET/4);
}

