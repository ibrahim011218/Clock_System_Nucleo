/*
 ******************************************************************************
 * @file           : Service.c
 * @Author         : MOHAMMEDs & HEMA
 * @brief          : Main program body
 * @Date           : Aug 30, 2023
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2023.
 * All rights reserved.
 *
 ******************************************************************************
 */

/* ========================================================================= *
 *                            INCLUDES SECTION                               *
 * ========================================================================= */

#include <stdint.h>

#include "../../Library/ErrTypes.h"
#include "../../Library/STM32F446xx.h"

#include "../../Drivers/Inc/RCC_Interface.h"
#include "../../Drivers/Inc/GPIO_Interface.h"
#include "../../Drivers/Inc/NVIC_Interface.h"
#include "../../Drivers/Inc/SCB_Interface.h"
#include "../../Drivers/Inc/DMA_Interface.h"
#include "../../Drivers/Inc/EXTI_Interface.h"
#include "../../Drivers/Inc/SYSCFG_Interface.h"
#include "../../Drivers/Inc/I2C_Interface.h"
#include "../../Drivers/Inc/SPI_Interface.h"
#include "../../Drivers/Inc/UART_Interface.h"
#include "../../Drivers/Inc/SYSTICK_Interface.h"

#include "../../HAL/Inc/DS1307_Interface.h"

#include "../Inc/Service.h"
#include "../Inc/Service_Private.h"

/* ========================================================================= *
 *                        GLOBAL VARIABLES SECTION                           *
 * ========================================================================= */

/* Variable to Send Via SPI Data in More than Case :
   1- Number of Tries of User is Finished
   2- System Login is Successfully Initiated
   2- Alarm is Triggered
   3- Display Date & Time
*/
uint8_t ReadingArr[30] = {0};

/* Variable to Put UART Configuration in IT to Be Used in Other Functions */
UART_Config_t *UART_CONFIG;

/* Variable to Put Date & Time Configuration in It to Send it to RTC
 *  to Set the Date & Time , Global To Extern it in APP Layer
 */
DS1307_Config_t Date_Time_RTC;

/* Variable to Put SPI Configuration in IT to Be Used in Other Functions */
SPI_CONFIGS_t *SPI_CONFIG;

/* Variable to Put I2C Configuration in IT to Be Used in Other Functions ,
  Global To Extern it in APP Layer */
I2C_Configs_t *I2C_CONFIG;

/* Variable to Put Date & Time Configuration in IT to Be Used in Displaying Date & Time */
DS1307_Config_t *ReadingStruct;

/* Alarm Time Array  */
uint8_t AlarmTime[5][3] = Filling;

/* Alarm Name Array */
uint8_t AlarmName[30] = {0};

/* Counter To Store The Alarm Name Length */
uint8_t AlarmNameCounter = 0;

/* ========================================================================= *
 *                    FUNCTIONS IMPLEMENTATION SECTION                       *
 * ========================================================================= */

/** ============================================================================
 * @fn 				: Wrong_OptionChoosen
 *
 * @brief 			: This Function is Called When User Passes Wrong Option to the System ,
 *                    It Clears the Terminal & Display a Message to the User to Enter a Valid Option
 *
 * @param[in]		: void
 *
 * @return 			: void
 * 					  This Function Never Returns
 * ============================================================================
 */
void WRONG_OptionChoosen(void)
{
	/* If Wrong Option is Provided to the System */
	/* New Line in Terminal */
	SendNew_Line();

	/* Notify User to Enter a Valid Option */
	USART_SendStringPolling(UART_CONFIG->UART_ID, "  Wrong Option , Enter Option (1-3) ");

	/* delay & clear terminal */
	DELAY_500ms();

	Clear_Terminal();
}

/** ============================================================================
 * @fn 				: Check_IF_ContinueisNeeded
 *
 * @brief 			: This Function is Called When User Finishes His Functionality( Executing Choosin Option )
 *                    and Asks Him if He Wants to Continue Choosing Options or Not
 *
 * @param[in]		: void
 *
 * @return 			: void
 * 					  This Function Never Returns
 * ============================================================================
 */
void Check_IF_ContinueisNeeded(void)
{
	/* Received Char From User After Making His Functionality */
	uint8_t Local_ReceivedChar = 0;

	/* Wait Until User Press 'y' */
	/* Send New Line to Terminal */
	SendNew_Line();

	/* Ask User if He Wants to Continue */
	USART_SendStringPolling(UART_CONFIG->UART_ID, "[+] Do you want to continue? [y/n] ");

	SendNew_Line();

	/* Receive User's Choice */
	Local_ReceivedChar = UART_u16Receive(UART_CONFIG);

	/* Transmit the Received Data to Visualize it on Putty Terminal */
	UART_voidTransmitData(UART_CONFIG, Local_ReceivedChar);

	/* If User Pressed 'n' Or 'N' or Any Other Character other Than 'y' & 'Y'
	 *  End the Program */
	if (Local_ReceivedChar == 'n' || Local_ReceivedChar == 'N' || (Local_ReceivedChar != 'y' && Local_ReceivedChar != 'Y'))
	{
		/* Clear Terminal */
		Clear_Terminal();

		/* Send Good Bye Message to User on Terminal */
		USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"┌──────────── •✧✧• ────────────┐\n");
		USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"-           Bye Bye :)         - \n");
		USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"└──────────── •✧✧• ────────────┘\n");

		/* Stuck in Infinite Loop */
		while (1)
			;
	}
	else
	{
		/* IF User Pressed 'y' or 'Y' ( Continue is Needed ) */
		/* Clear Terminal & Continue the Program */
		Clear_Terminal();
	}
}

/** ============================================================================
 * @fn 				: Check_LoginInfo
 *
 * @brief 			: This Function is Called When User Enters the System ,
 *                    It Checks on the ID & Password if ID is Equal to Password Inverted or Not
 * 				      if ID is Equal to Password Inverted , User Enters the System ,
 *                    if Not , User is Asked to Enter ID & Password Again Till he Finish His Number of Tries
 *          	   	  if Number of Tries is Finished , SPI Data is Sent to Light Up the Red LED on BluePill Board,
 *                    IN this Case SPI Call Back Function is Called to Execute the Shutdown Sequence
 *                    and the Program is Stuck in it
 *
 * @param[in]		:  uint8_t *ID_Ptr > Pointer to ID Array
 * 					   uint8_t *Pass_Ptr > Pointer to Password Array
 * 					   uint8_t TriesNumber > Number of Tries Allowed to User to Enter the System Before Shutting Down
 *
 * @return 			: void
 * 					  This Function Never Returns
 * ============================================================================
 */
void Check_LoginInfo(uint8_t *ID_Ptr, uint8_t *Pass_Ptr, uint8_t TriesNumber)
{
	/* Data To Send Via SPI if Number of Tries is Finished */
	uint8_t DATA_SENT_viaSPI[30] = {RED_LED_CODE};

	/* Variable to Hold Return of Function Checking on ID & Inverted Pass */
	ID_PASS_EQUALITY_t ID_PASS_Relation = ID_NOEQUAL_INVERTED_PASS;

	/* Check if ID Equals Password Inverted */
	ID_PASS_Relation = ID_Equal_InvertedPass(ID_Ptr, Pass_Ptr, NUM_OF_ID_PASS_DIGITS);

	/* Loop Terminates if Number of Tries is Finished */
	while (TriesNumber > 0)
	{
		/* IF ID is Not Equal to Inverted Password */
		if (ID_PASS_Relation == ID_NOEQUAL_INVERTED_PASS)
		{
			/* Decrement Number of Tries */
			TriesNumber--;

			/* Clear Putty Terminal */
			Clear_Terminal();

			/* If Number of Tries is Finished Terminate the While Loop */
			if (TriesNumber == 0)
			{
				break;
			}

			/* Take ID & Password again From User for The Next Try and Check on Them
			 * if ID is Equal To Password Inverted
			 */
			ID_PASS_Relation = TryAgain(ID_Ptr, Pass_Ptr);
		}
		/* IF ID is Equal to the Password Inverted Terminate The Loop */
		else if (ID_PASS_Relation == ID_EQUAL_INVERTED_PASS)
		{
			break;
		}
	}

	/* Clear Putty Terminal */
	Clear_Terminal();

	/* Loop is Terminated Because Number of Tries is Finished */
	if (TriesNumber == 0)
	{
		/* Send A Signal To Light Up the Red LED ON BluePill Board */
		SPI_Transmit_IT(SPI_CONFIG, DATA_SENT_viaSPI, 30, SPI_CallBackFunc);

		/* Execute Shutdown Sequence in SPI Call Back Function & Stuck in it */
	}
	else
	{
		/* Loop is Terminated Because ID is Equal to Password Inverted */
		/* Number of Tries Not Finished */
		/* Send a Signal to Turn on Green LED on Panda board*/
		SendGreenSignal( ) ;
	}
}

/** ============================================================================
 * @fn 				: Display_Menu
 *
 * @brief 			: This Function is Called When User Enters the System , It Displays the Menu to the User
 *                    and Asks Him to Choose From the Following Options :
 *                    1- Display Date & Time
 * 				      2- Set Alarm
 * 				      3- Set Date & Time
 *
 * @param[in]		: void
 *
 * @return 			: OPTIONS_t > Option Choosen by User ( 1-3 )
 *
 * @note			: This Function is Called Only Once When User Enters the System ( ID & Password are Correct )
 * ============================================================================
 */
OPTIONS_t Display_Menu(void)
{
	/* Variable to Hold the Option Choosen by User */
	OPTIONS_t ChoosenOption = NO_OPTION;

	/* Welcome Message */
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"┌──────────── •✧✧• ────────────┐\n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"-  Welcome To My Clock System  - \n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"└──────────── •✧✧• ────────────┘\n");

	/* New Line in Terminal */
	SendNew_Line();

	/* Instructions to User */
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"  ========================================================================\n ");
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"||                     Choose From The Following Menu :                 ||  ");
	SendNew_Line();
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"  ========================================================================\n");

	/* Display Menu to User */
	USART_SendStringPolling(UART_CONFIG->UART_ID, "1- Display Date & Time  \n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, "2- Set Alarm            \n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, "3- Set Date & Time      \n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, "[+] select option (1-3) : ");

	/* Receive Option From User */
	ChoosenOption = (uint8_t)UART_u16Receive(UART_CONFIG);

	/* Display Choosen Option in Putty Terminal */
	UART_voidTransmitData(UART_CONFIG, ChoosenOption);

	/* Return Option Choosen by User */
	return ChoosenOption;
}

/** ============================================================================
 * @fn 				: SendNew_Line
 *
 * @brief 			: This Function is Used to Send New Line in Putty Terminal

 * @param[in]		: void
 *
 * @return 			: void
 *
 * ============================================================================
 */
void SendNew_Line(void)
{
	/* Send New Line In Terminal */
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"\n");
}

/** ============================================================================
 * @fn 				: Clear_Terminal
 *
 * @brief 			: This Function is Used to Clear Putty Terminal

 * @param[in]		: void
 *
 * @return 			: void
 *
 * ============================================================================
 */
void Clear_Terminal(void)
{
	/* Clear Putty Terminal */
	USART_SendStringPolling(UART_CONFIG->UART_ID, CLEAR_TERMINAL);
}

/** ============================================================================
 * @fn 				: ID_Reception
 *
 * @brief 			: This Function is Used to Receive ID From User Digit By Digit
 *
 * @param[in]		: void
 *
 * @return 			: uint8_t *ID_Ptr > Pointer to ID Array
 *
 * ============================================================================
 */
uint8_t *ID_Reception(void)
{
	/* Array to Store the ID */
	static uint8_t ID[NUM_OF_ID_PASS_DIGITS] = {0};

	/* For Loop Counter */
	uint8_t Local_u8Counter = 0;

	/* Display to the User Text to Enter the ID */
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"Enter 4 Digits ID : ");

	/* Receive ID From User */
	for (Local_u8Counter = 0; Local_u8Counter < NUM_OF_ID_PASS_DIGITS; Local_u8Counter++)
	{
		/* Receive ID From User Digit By Digit */
		ID[Local_u8Counter] = UART_u16Receive(UART_CONFIG);

		/* Transmit the Received Data to Visualize it on Putty */
		UART_voidTransmitData(UART_CONFIG, ID[Local_u8Counter]);
	}

	/* Send New Line to the terminal */
	SendNew_Line();

	/* Return ID Array */
	return ID;
}

/** ============================================================================
 * @fn 				: Pass_Reception
 *
 * @brief 			: This Function is Used to Receive Password From User Digit By Digit
 *
 * @param[in]		: void
 *
 * @return 			: uint8_t *Pass_Ptr > Pointer to Password Array
 *
 * ============================================================================
 */
uint8_t *Pass_Reception(void)
{
	/* Array to Store the Password */
	static uint8_t Pass[NUM_OF_ID_PASS_DIGITS] = {0};

	/* For Loop Counter */
	uint8_t Local_u8Counter = 0;

	/* Display to the User Text to Enter the ID */
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"Enter Password : ");

	/* Receive Pass From User */
	for (Local_u8Counter = 0; Local_u8Counter < NUM_OF_ID_PASS_DIGITS; Local_u8Counter++)
	{
		/* Receive Pass From User Digit By Digit */
		Pass[Local_u8Counter] = UART_u16Receive(UART_CONFIG);

		/* Transmit the * to Make the Password Invisible */
		UART_voidTransmitData(UART_CONFIG, '*');
	}

	/* Small Delay to Visualize the Last * on Putty Terminal */
	DELAY_500ms();

	/* Send New Line to the terminal */
	SendNew_Line();

	/* Return Password Array */
	return Pass;
}

/*=======================================================================================
 * @fn		 		:	Clock_Init
 * @brief			:	Enable The Pripherals Clocks
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void Clock_Init(void)
{
	/* Enable USART2 Clock */
	RCC_APB1EnableCLK(USART2EN);

	/* Enable SPI1 Clock */
	RCC_APB2EnableCLK(SPI1EN);

	/* Enable GPIO PortA Clock */
	RCC_AHB1EnableCLK(GPIOAEN);

	/* Enable GPIO PortB Clock */
	RCC_AHB1EnableCLK(GPIOBEN);

	/* Enable I2C1 Clock */
	RCC_APB1EnableCLK(I2C1EN);
}

/*=======================================================================================
 * @fn		 		:	Pins_Init
 * @brief			:	Set The Pins Configurations
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void Pins_Init(void)
{
	/* USART2 GPIO Pins Configuration Working in Full Duplex */
	GPIO_PinConfig_t USART2_Pins[NUM_OF_USART_PINS] =
		{
			/* USART2 Tx Pin */
			{.AltFunc = AF7, .Mode = ALTERNATE_FUNCTION, .OutputType = PUSH_PULL, .PinNum = PIN2, .Port = PORTA, .PullType = NO_PULL, .Speed = LOW_SPEED},
			/* USART2 Rx Pin */
			{.AltFunc = AF7, .Mode = ALTERNATE_FUNCTION, .OutputType = PUSH_PULL, .PinNum = PIN3, .Port = PORTA, .PullType = NO_PULL, .Speed = LOW_SPEED}};

	/* Initializing USART2 Pins */
	GPIO_u8PinsInit(USART2_Pins, NUM_OF_USART_PINS);

	/* SPI1 GPIO Pins Configuration Working in Simplex */
	GPIO_PinConfig_t SPI1_Pins[NUM_OF_SPI_PINS] =
		{
			/* SPI1 MOSI Pin */
			{.AltFunc = AF5, .Mode = ALTERNATE_FUNCTION, .OutputType = PUSH_PULL, .PinNum = PIN7, .Port = PORTA, .PullType = NO_PULL, .Speed = LOW_SPEED},
			/* SPI1 SCK Pin */
			{.AltFunc = AF5, .Mode = ALTERNATE_FUNCTION, .OutputType = PUSH_PULL, .PinNum = PIN5, .Port = PORTA, .PullType = NO_PULL, .Speed = LOW_SPEED},
			/* SPI1 NSS Pin */
			{.AltFunc = AF5, .Mode = ALTERNATE_FUNCTION, .OutputType = PUSH_PULL, .PinNum = PIN4, .Port = PORTA, .PullType = PULL_UP, .Speed = LOW_SPEED}};

	/* Initializing SPI1 Pins */
	GPIO_u8PinsInit(SPI1_Pins, NUM_OF_SPI_PINS);

	/*I2C GPIO Pins Configurations*/
	GPIO_PinConfig_t I2C1_Pins[NUM_OF_I2C_PINS] = {
		{.Mode = ALTERNATE_FUNCTION,
		 .AltFunc = AF4,
		 .OutputType = OPEN_DRAIN,
		 .PullType = PULL_UP,
		 .Port = PORTB,
		 .PinNum = PIN8,
		 .Speed = LOW_SPEED},
		{.Mode = ALTERNATE_FUNCTION,
		 .AltFunc = AF4,
		 .OutputType = OPEN_DRAIN,
		 .PullType = PULL_UP,
		 .Port = PORTB,
		 .PinNum = PIN9,
		 .Speed = LOW_SPEED}};

	/* Initializing I2C1 Pins */
	GPIO_u8PinsInit(I2C1_Pins, NUM_OF_I2C_PINS);

	GPIO_PinConfig_t PB6_EXTI =
		{
			.Port = PORTB,
			.PinNum = PIN6,
			.Mode = OUTPUT,
			.OutputType = PUSH_PULL,
			.PullType = PULL_DOWN,

		};

	/* Initialize Pin PB6 For EXTI */
	GPIO_u8PinInit(&PB6_EXTI);
}

/*=======================================================================================
 * @fn		 		:	USART2_Init
 * @brief			:	Initialize USART2
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void USART2_Init(void)
{
	/* USART2 Interrupts Configuration */
	static UART_Interrupts_t USART2Interrupts =
		{
			.IDLE = UART_Disable, .PE = UART_Disable, .RXN = UART_Disable, .TC = UART_Disable, .TX = UART_Disable

		};

	/* USART2 Configuration */
	static UART_Config_t USART2Config =
		{
			.UART_ID = UART_2, .BaudRate = BaudRate_9600, .Direction = RX_TX, .OverSampling = OverSamplingBy16, .ParityState = UART_Disable, .StopBits = OneStopBit, .WordLength = _8Data, .Interrupts = &USART2Interrupts};

	/* USART2 Initialization */
	UART_voidInit(&USART2Config);

	/* Initialize UART Struct Globally */
	UART_CONFIG = &USART2Config;
}

/*==============================================================================================================================================
 *@fn      : void InterruptsInit (void)
 *@brief  :  This Function Is Responsible For Initializing The Interrupts
 *@retval void :
 *==============================================================================================================================================*/
void Interrupts_Init(void)
{
	NVIC_EnableIRQ(SPI1_IRQ);

	/* Set 2 Group Priorities & 8 Sub Priorities*/
	SCB_VoidSetPriorityGroup(GP_2_SP_8);

	/* Set SPI to Group Priority Zero*/
	NVIC_SetPriority(SPI1_IRQ, 0);

	/* Set SYSTICK to Group Priority One*/
	SCB_VoidSetCorePriority(SYSTICK_FAULT, (1 << 7));
}

/*=======================================================================================
 * @fn		 		:	SPI1_Init
 * @brief			:	Initialize SPI1
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void SPI1_Init(void)
{
	/* SPI1 Configuration */
	static SPI_CONFIGS_t SPI1Config =
		{
			.BaudRate_Value = BAUDRATE_FpclkBY256, .CRC_State = CRC_STATE_DISABLED, .Chip_Mode = CHIP_MODE_MASTER, .Clock_Phase = CLOCK_PHASE_CAPTURE_FIRST, .Clock_Polarity = CLOCK_POLARITY_IDLE_LOW, .Frame_Size = DATA_FRAME_SIZE_8BITS, .Frame_Type = FRAME_FORMAT_MSB_FIRST, .MultiMaster_State = MULTIMASTER_PROVIDED, .SPI_Num = SPI_NUMBER1, .Transfer_Mode = TRANSFER_MODE_FULL_DUPLEX};

	/* SPI1 Initialization */
	SPI_Init(&SPI1Config);

	/* Initialize SPI Struct Globally */
	SPI_CONFIG = &SPI1Config;
}

/*=======================================================================================
 * @fn		 		:	I2C1_Init
 * @brief			:	Initialize I2C1
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void I2C1_Init(void)
{

	/*I2C1 Configurations*/
	static I2C_Configs_t _I2C1 = {
		.ADD_Mode = ADDRESSING_MODE_7BITS,
		.Chip_Address = 10,
		.I2C_Mode = MASTER_MODE_STANDARD,
		.I2C_Num = I2C_NUMBER_1,
		.I2C_Pclk_MHZ = 16,
		.PEC_State = PACKET_ERR_CHECK_DISABLED,
		.SCL_Frequency_KHZ = 100,
		.Stretch_state = CLK_STRETCH_ENABLED};

	/* I2C1 Initialization */
	I2C_Init(&_I2C1);

	I2C_CONFIG = &_I2C1;
}

/** ============================================================================
 * @fn 				: ShutDown_Sequence
 *
 * @brief 			: This Function is Called Inside Call Back Function of SPI ,
 *                    When Number of Tries of User is Finished &
 *                    RED_LED_CODE is Transmitted to the Blue Pill Board ,
 *                    This Function is Called to Execute the Shutdown Sequence
 *
 * @param[in]		: void
 *
 * @return 			: void
 *
 * ============================================================================
 */
void ShutDown_Sequence(void)
{
	/* This Funciton is Called When Number of Tries of User is Finished & RED_LED_CODE is Transmitted to the
	 * Blue Pill Board
	 */
	/* Clear Putty Terminal */
	Clear_Terminal();

	/* Send Shut Down Message to User on Terminal */
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"┌──────────── •✧✧• ────────────┐\n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"-     System Shut Down         - \n");
	USART_SendStringPolling(UART_CONFIG->UART_ID, (char *)"└──────────── •✧✧• ────────────┘\n");

	/* Stuck in Infinite Loop */
	while (1)
		;
}

/*=======================================================================================
 * @fn		 		:	ReadDateTime_FromPC
 * @brief			:	Read Date & Time From The user Via USART
 * @param			:	void
 * @retval			:	Error State
 * ======================================================================================*/
Error_State_t ReadDateTime_FromPC(void)
{
	/*Error State Variable to check if the functionality is done successfully or not*/
	Error_State_t Error_State = OK;

	/*Array to store the Date and Time Received from the user*/
	uint8_t Date_Time_USART[CALENDER_FORMAT] = {0};

	/*Variable to check if this is the first time to enter this function or not*/
	static uint8_t First_Time_Flag = FIRST_TIME;

	/*Check if this is the first time to enter this function or not*/
	if (First_Time_Flag == FIRST_TIME)
	{
		/*Display message to user that he is in the Set Date and Time Mode*/
		USART_SendStringPolling(UART_2, "\nWELCOME To Set Date and Time Mode\n");
		/*Change the flag to not enter this if statement again*/
		First_Time_Flag = NOT_FIRST_TIME;
	}
	/*Display message to user that he should enter the Date and Time in the following form*/
	USART_SendStringPolling(UART_2, "Enter the Date And time in the Following Form\n");
	USART_SendStringPolling(UART_2, "yy-mm-dd (First 3 Letters of Day Name) HH:MM:SS\n");

	/*Receive the Date and Time from the user*/
	for (uint8_t Local_Counter = 0; Local_Counter < CALENDER_FORMAT; Local_Counter++)
	{
		Date_Time_USART[Local_Counter] = UART_u16Receive(UART_CONFIG);
		UART_voidTransmitData(UART_CONFIG, Date_Time_USART[Local_Counter]);
	}
	/*Calculate calender Values to be send to RTC*/
	Calculate_Calender(&Date_Time_RTC, Date_Time_USART);

	/*Estimate the Day Name*/
	Date_Time_RTC.Day = FindDay(Date_Time_USART);

	/*Check the given Calender*/
	if (OK == Check_Calender(&Date_Time_RTC))
	{
		Error_State = OK;
	}
	else
	{
		Error_State = NOK;
	}

	return Error_State;
}

/*=======================================================================================
 * @fn		 		:	Transmit_Time
 * @brief			:	Transmit Buffer of data via SPI (with interrupt)
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void Transmit_Time(void)
{
	/* Transmit Time Via SPI */
	SPI_Transmit_IT(SPI_CONFIG, ReadingArr, 30, SPI_CALL_BACK);
}

/*=======================================================================================
 * @fn		 		:	Reading_Time
 * @brief			:	Reading data from RTC and store data in Reading Array
 * @param			:	void
 * @retval			:	void
 * ======================================================================================*/
void Reading_Time(void)
{
	/* Read Date & Time */
	ReadingStruct = DS1307_ReadDateTime(I2C_CONFIG);

	/* Convert Reading Struct into Reading Array */

	ReadingArr[0] = DISPLAY_CODE;
	ReadingArr[1] = ReadingStruct->Seconds;
	ReadingArr[2] = ReadingStruct->Minutes;
	ReadingArr[3] = ReadingStruct->Hours;
	ReadingArr[4] = ReadingStruct->Day;
	ReadingArr[5] = ReadingStruct->Month;
	ReadingArr[6] = ReadingStruct->Year;
	ReadingArr[7] = ReadingStruct->Date;
}

/*==============================================================================================================================================
 *@fn      : void CalcAlarm(uint8_t AlarmNumber)
 *@brief  :  This Function Is Responsible For Calculating The Alarm Time And Storing It In The Global Array
 *@paramter[in]  : uint8_t AlarmNumber : The Number Of The Alarm To Be Set
 *@retval void :
 *==============================================================================================================================================*/
void CalcAlarm(uint8_t AlarmNumber)
{
	/* Variable To Store The Received Data From UART */
	uint8_t RecTemp[8] = {0};

	uint8_t LoopCounter = 0;

	/* Receive The Alarm Time From UART And Store It In The Array */
	for (LoopCounter = 0; LoopCounter < 8; LoopCounter++)
	{
		RecTemp[LoopCounter] = UART_u16Receive(UART_CONFIG);

		UART_voidTransmitData(UART_CONFIG, RecTemp[LoopCounter]);
	}

	/* Store The Received Data In The Global Array */
	AlarmTime[AlarmNumber - 48][0] = (RecTemp[0] - 48) * 10 + (RecTemp[1] - 48);
	AlarmTime[AlarmNumber - 48][1] = (RecTemp[3] - 48) * 10 + (RecTemp[4] - 48);
	AlarmTime[AlarmNumber - 48][2] = (RecTemp[6] - 48) * 10 + (RecTemp[7] - 48);
}

/*==============================================================================================================================================
 *@fn      :  void CompTime()
 *@brief  :   This Function Is Responsible For Comparing The Current Time With The Alarm Time And Send The Alarm Number To The Blue Pill If They Are Equal
 *@retval void :
 *==============================================================================================================================================*/
void CompTime()
{
	/* Variable To Store The Received Time From The RTC */
	DS1307_Config_t *RecievedTime;

	/* Reading The Current Time From The RTC */
	RecievedTime = DS1307_ReadDateTime(I2C_CONFIG);

	/* Array To Store The Current Time From The Current Time Recieved From The RTC */
	uint8_t CurrentTime[3] = {
		RecievedTime->Hours, RecievedTime->Minutes, RecievedTime->Seconds};

	/* Variable To Loop On The Alarm Time Array  Counter1 For Looping On Alarm Number & Counter2 For Looping On The Alarm Time */
	uint8_t Counter1, Counter2 = 0;

	/* Variable To Check The Equality Between The Current Time And The Alarm Time */
	Equality_t EqualityCheck = NotEqual;

	/* Loop On The Alarm Number */
	for (Counter1 = 0; Counter1 < 5; Counter1++)
	{
		/* Check If The Alarm Time Is Not Empty */
		if (AlarmTime[Counter1][0] != 0xFF)
		{
			/* Loop On The Alarm Time */
			for (Counter2 = 0; Counter2 < 3; Counter2++)
			{
				/* Check If The Current Time Is Equal To The Alarm Time */
				if (AlarmTime[Counter1][Counter2] != CurrentTime[Counter2])
				{
					/* If Not Equal Break The Loop And Continue To The Next Alarm Number */
					EqualityCheck = NotEqual;
					break;
				}
				/* If Equal Continue The Loop */
				else
				{
					/* If The Loop Reached The End Of The Alarm Time Array Then The Current Time Is Equal To The Alarm Time */
					EqualityCheck = Equal;
				}
			}
		}
		/* If The Alarm Time Is Empty Continue To The Next Alarm Number */
		else
		{
			continue;
		}
		/* If The Current Time Is Equal To The Alarm Time Send The Alarm Number To The Blue Pill */
		if (EqualityCheck == Equal)
		{
			AlarmName[0] = ALARMCODE;
			/* Variable To Store The Alarm Number */
			AlarmName[1] = ++Counter1;
			/* Send The Alarm Number To The Blue Pill */
			SPI_Transmit_IT(SPI_CONFIG, AlarmName, 30, &SPI1_ISR);
		}
	}
}

/*==============================================================================================================================================
 *@fn      : void SPI1_ISR()
 *@brief  :  This Function Is The ISR For The SPI1 Interrupt
 *@retval void :
 *==============================================================================================================================================*/
void SPI1_ISR()
{
	/* Notify The Blue Pill That The Alarm Is Fired */
	GPIO_u8SetPinValue(PORTB, PIN6, PIN_HIGH);
	DELAY_500ms();
	GPIO_u8SetPinValue(PORTB, PIN6, PIN_LOW);
}

/*==============================================================================================================================================
 *@fn      : void SetAlarm()
 *@brief  :  This Function Is Responsible For Setting The Alarm Time
 *@retval void :
 *==============================================================================================================================================*/
void SetAlarm()
{
	/* Variable To Store The Alarm Number */
	uint8_t ChooseNum = 0;

	SendNew_Line();

	/* Ask The User To Choose The Alarm Number */
	USART_SendStringPolling(UART_2, "Please Choose Alarm Number From ( 1 ~ 5 )\nYour Choice: ");

	/* Receive The Alarm Number From The User */
	ChooseNum = UART_u16Receive(UART_CONFIG);

	/* To Print on Terminal What User Typed */
	UART_voidTransmitData(UART_CONFIG, ChooseNum);

	SendNew_Line();

	/* Ask The User To Enter The Alarm Name */
	USART_SendStringPolling(UART_2, "Please Enter Alarm Name: ");

	/* Loop To Receive The Alarm Name From The User Until The User Press Enter */
	for (AlarmNameCounter = 2; AlarmNameCounter < 30; AlarmNameCounter++)
	{

		/* Receive The Alarm Name From The User */
		AlarmName[AlarmNameCounter] = UART_u16Receive(UART_CONFIG);
		if (AlarmName[AlarmNameCounter] == 13)
		{
			break;
		}
		UART_voidTransmitData(UART_CONFIG, AlarmName[AlarmNameCounter]);
	}

	SendNew_Line();

	/* Check If The Alarm Number Is In The Range */
	if (ChooseNum > '0' && ChooseNum < '6')
	{
		USART_SendStringPolling(UART_2, "Please Enter Your Alarm in this sequence xx:xx:xx\n");
		CalcAlarm(ChooseNum - 1);
	}
	/* If The Alarm Number Is Not In The Range Send Wrong Choice To The User */
	else
	{
		/* Send Wrong Choice To The User */
		USART_SendStringPolling(UART_2, " Wrong Choice ");
		/* Ask The User To Choose The Alarm Number Again */
		SetAlarm();
	}
}

/*==============================================================================================================================================
 *@fn      :  void SendGreenSignal()
 *@brief  :   This Function Is Responsible For Sending a Signal to Panda Board when System Login is Completed
 *@retval void :
 *==============================================================================================================================================*/
void SendGreenSignal( void )
{
	ReadingArr[0] = GREEN_LED_CODE ;
	SPI_Transmit_IT(SPI_CONFIG, ReadingArr , 30 , SPI_CALL_BACK) ;
}

/* ============================================================================*
 * 								Private Functions							   *
 * ============================================================================*/

/*=======================================================================================
 * @fn		 		:	FindDay
 * @brief			:	Find The Day given by the user
 * @param			:	Pointer to the Calender Array received from user
 * @retval			:	The Day received from the user
 * ======================================================================================*/
static DS1307_DAYS_t FindDay(uint8_t *Calender)
{
	/*Variable to store the Day*/
	DS1307_DAYS_t Day = 0;

	/*Check the 3 Given Letters*/
	if ((('S' == Calender[FIRST_LETTER_OF_DAY]) || ('s' == Calender[FIRST_LETTER_OF_DAY])) && (('A' == Calender[SECOND_LETTER_OF_DAY]) || ('a' == Calender[SECOND_LETTER_OF_DAY])) && (('T' == Calender[THIRD_LETTER_OF_DAY]) || ('t' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are SAT so the day is Saturday*/
		Day = DS1307_SATURDAY;
	}
	else if ((('S' == Calender[FIRST_LETTER_OF_DAY]) || ('s' == Calender[FIRST_LETTER_OF_DAY])) && (('U' == Calender[SECOND_LETTER_OF_DAY]) || ('u' == Calender[SECOND_LETTER_OF_DAY])) && (('N' == Calender[THIRD_LETTER_OF_DAY]) || ('n' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are SUN so the day is Sunday*/
		Day = DS1307_SUNDAY;
	}
	else if ((('M' == Calender[FIRST_LETTER_OF_DAY]) || ('m' == Calender[FIRST_LETTER_OF_DAY])) && (('O' == Calender[SECOND_LETTER_OF_DAY]) || ('o' == Calender[SECOND_LETTER_OF_DAY])) && (('N' == Calender[THIRD_LETTER_OF_DAY]) || ('n' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are MON so the day is Monday*/
		Day = DS1307_MONDAY;
	}

	else if ((('T' == Calender[FIRST_LETTER_OF_DAY]) || ('t' == Calender[FIRST_LETTER_OF_DAY])) && (('U' == Calender[SECOND_LETTER_OF_DAY]) || ('u' == Calender[SECOND_LETTER_OF_DAY])) && (('E' == Calender[THIRD_LETTER_OF_DAY]) || ('e' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are Tue so the day is Tuesday*/
		Day = DS1307_TUESDAY;
	}
	else if ((('W' == Calender[FIRST_LETTER_OF_DAY]) || ('w' == Calender[FIRST_LETTER_OF_DAY])) && (('E' == Calender[SECOND_LETTER_OF_DAY]) || ('e' == Calender[SECOND_LETTER_OF_DAY])) && (('D' == Calender[THIRD_LETTER_OF_DAY]) || ('d' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are wed so the day is Wednesday*/
		Day = DS1307_WEDNESDAY;
	}
	else if ((('T' == Calender[FIRST_LETTER_OF_DAY]) || ('t' == Calender[FIRST_LETTER_OF_DAY])) && (('H' == Calender[SECOND_LETTER_OF_DAY]) || ('h' == Calender[SECOND_LETTER_OF_DAY])) && (('U' == Calender[THIRD_LETTER_OF_DAY]) || ('u' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are THU so the day is Thrusday*/
		Day = DS1307_THURSDAY;
	}
	else if ((('F' == Calender[FIRST_LETTER_OF_DAY]) || ('f' == Calender[FIRST_LETTER_OF_DAY])) && (('R' == Calender[SECOND_LETTER_OF_DAY]) || ('r' == Calender[SECOND_LETTER_OF_DAY])) && (('I' == Calender[THIRD_LETTER_OF_DAY]) || ('i' == Calender[THIRD_LETTER_OF_DAY])))
	{
		/*First 3 letters are FRI so the day is Friday*/
		Day = DS1307_FRIDAY;
	}
	else
	{
		/*Wrong Day*/
		Day = WRONG_DAY;
	}
	/*Return the Day*/
	return Day;
}

/*=======================================================================================
 * @fn		 		:	Calculate_Calender
 * @brief			:	Translate The Calender given by the user from ASCII to Decimal values
 * 						And save it in a structure which will be sent to RTC.
 * @param			:	Pointer to the Calender struct which will be sent to RTC
 * @param			:	Pointer to the Calender Array received from user
 * @retval			:	void
 * ======================================================================================*/
static void Calculate_Calender(DS1307_Config_t *Date_Time_To_RTC, uint8_t *Date_Time_From_USART)
{
	/*Calculate The Date*/
	Date_Time_To_RTC->Year = (((Date_Time_From_USART[0] - ZERO_ASCII) * 10) + (Date_Time_From_USART[1] - ZERO_ASCII));
	Date_Time_To_RTC->Month = (((Date_Time_From_USART[3] - ZERO_ASCII) * 10) + (Date_Time_From_USART[4] - ZERO_ASCII));
	Date_Time_To_RTC->Date = (((Date_Time_From_USART[6] - ZERO_ASCII) * 10) + (Date_Time_From_USART[7] - ZERO_ASCII));

	/*Calculate the Time*/
	Date_Time_To_RTC->Hours = (((Date_Time_From_USART[15] - ZERO_ASCII) * 10) + (Date_Time_From_USART[16] - ZERO_ASCII));
	Date_Time_To_RTC->Minutes = (((Date_Time_From_USART[18] - ZERO_ASCII) * 10) + (Date_Time_From_USART[19] - ZERO_ASCII));
	Date_Time_To_RTC->Seconds = (((Date_Time_From_USART[21] - ZERO_ASCII) * 10) + (Date_Time_From_USART[22] - ZERO_ASCII));
}

/*=======================================================================================
 * @fn		 		:	Check_Calender
 * @brief			:	Check The Calender given by the user
 * @param			:	Pointer to the Calender struct which will be sent to RTC
 * @retval			:	Error State
 * ======================================================================================*/
static Error_State_t Check_Calender(DS1307_Config_t *Date_Time_To_RTC)
{
	Error_State_t Error_State = OK;
	/*Check if the Date and Time are in the Acceptable Range*/
	if ((Date_Time_To_RTC->Date > MAX_DATE) || (Date_Time_To_RTC->Day == WRONG_DAY) || (Date_Time_To_RTC->Hours > MAX_HOURS) || (Date_Time_To_RTC->Minutes > MAX_MINUTES) || (Date_Time_To_RTC->Month > MAX_MONTH) || (Date_Time_To_RTC->Seconds > MAX_SECONDS) || (Date_Time_To_RTC->Year > MAX_YEAR))
	{
		Error_State = NOK;
	}
	return Error_State;
}

/** ============================================================================
 * @fn 				: TryAgain
 *
 * @brief 			: This Function is Called When User Enters Wrong ID & Password ,
 *                    It Asks User to Enter ID & Password Again
 *
 * @param[in]		: uint8_t *ID_Ptr > Pointer to ID Array
 *                    uint8_t *Pass_Ptr > Pointer to Password Array
 *
 * @return 			: ID_PASS_EQUALITY_t > ( ID_EQUAL_INVERTED_PASS ) if ID is Equal to Password Inverted
 *                    ( ID_NOEQUAL_INVERTED_PASS ) if ID is Not Equal to Password Inverted
 *
 * @note			: This Function is Called in Check_LoginInfo Function( Private Function )
 * ============================================================================
 */
static ID_PASS_EQUALITY_t TryAgain(uint8_t *ID_Ptr, uint8_t *Pass_Ptr)
{
	ID_PASS_EQUALITY_t ID_PASS_Relation = ID_NOEQUAL_INVERTED_PASS;

	/* Receive ID From User */
	ID_Ptr = ID_Reception();

	/* Receive Password From User */
	Pass_Ptr = Pass_Reception();

	/* Check if ID is Equal to Inverted Password */
	ID_PASS_Relation = ID_Equal_InvertedPass(ID_Ptr, Pass_Ptr, NUM_OF_ID_PASS_DIGITS);

	return ID_PASS_Relation;
}

/** ============================================================================
 * @fn 				: ID_Equal_InvertedPass
 *
 * @brief 			: This Function is Called to Check if ID is Equal to Password Inverted or Not
 *
 * @param[in]		: uint8_t *ID > Pointer to ID Array
 *     			      uint8_t *Pass > Pointer to Password Array
 *    		 		  uint8_t Size > Size of ID & Password Arrays
 *
 * @return 			: ID_PASS_EQUALITY_t > ( ID_EQUAL_INVERTED_PASS ) if ID is Equal to Password Inverted
 *  				( ID_NOEQUAL_INVERTED_PASS ) if ID is Not Equal to Password Inverted
 *
 * @note			: This Function is Called in TryAgain Function( Private Function )
 *
 * ============================================================================
 */
static ID_PASS_EQUALITY_t ID_Equal_InvertedPass(uint8_t *ID, uint8_t *Pass, uint8_t Size)
{
	/* Variable that Holds the Value if ID Equals Password Inverted */
	ID_PASS_EQUALITY_t ID_InvertedPass_Relation = ID_NOEQUAL_INVERTED_PASS;

	/* Loop Counter */
	uint8_t Local_u8Counter = 0;

	/* Invert Password */
	uint8_t *InvertedPass = InvertPass(Pass, Size);

	/* Check on Every Element if ID Equals Inverted Password*/
	for (Local_u8Counter = 0; Local_u8Counter < Size; Local_u8Counter++)
	{
		/* IF Digit From ID Equals Digit From Inverted Password */
		if (ID[Local_u8Counter] == InvertedPass[Local_u8Counter])
		{
			ID_InvertedPass_Relation = ID_EQUAL_INVERTED_PASS;
		}
		else
		{
			/* IF One Digit is Not Equal */
			ID_InvertedPass_Relation = ID_NOEQUAL_INVERTED_PASS;
			break;
		}
	}
	return ID_InvertedPass_Relation;
}

/** ============================================================================
 * @fn 				: InvertPass
 *
 * @brief 			: This Function is Called Used to Invert the Password Entered by User
 *
 * @param[in]		: uint8_t *Arr > Pointer to ID Array
 * 					  uint8_t ArrSize > Size of ID Array
 *
 * @return 			: uint8_t * > Pointer to Inverted Password Array
 *
 * @note			: This Function is Called in ID_Equal_InvertedPass Function( Private Function )
 * ============================================================================
 */
static uint8_t *InvertPass(uint8_t *Arr, uint8_t ArrSize)
{
	/* Loop Counter */
	uint8_t Local_u8Counter = 0;

	/* Array to Hold Password After Being Inverted */
	static uint8_t InvertedArr[NUM_OF_ID_PASS_DIGITS] = {0};

	/* Inverting Algorithm */
	for (Local_u8Counter = 0; Local_u8Counter < ArrSize; Local_u8Counter++)
	{
		InvertedArr[Local_u8Counter] = Arr[ArrSize - Local_u8Counter - 1];
	}

	/* Return Pointer to Inverted Password Array */
	return InvertedArr;
}

/* ============================================================================*
 * 								ISRs  										   *
 * ============================================================================*/
/* Unused Call Back Function of SPI , When Transmitting Data to Display  & when Transmitting Green Led Signal */
void SPI_CALL_BACK(void)
{
}
