
#ifndef AD7091R_H
	#define AD7091R_H

	// подключение заголовочных файлов модулей проекта
	#include "link.h"
	
	
	#define AD7091R_NUMBER_OF_CHIPS					1
	#define AD7091R_NUMBER_OF_CHANNELS			8
	
	#define AD7091R_SPI											MDR_SSP2
	#define AD7091R_SPI_WRITE_CMD						(uint16_t)0x0400
	#define AD7091R_SPI_READ_CMD						(uint16_t)0x0000
	
	// REGISTER ADDRESS
	#define	AD7091R_CONVERSION_RESULT_REG									((uint16_t)0x00)
	#define	AD7091R_CHANNEL_REG														((uint16_t)0x01)
	#define	AD7091R_CONFIGURATION_REG											((uint16_t)0x02)
	#define	AD7091R_ALERT_INDICATION_REG									((uint16_t)0x03)
		
	// CHANNEL REGISTER
	#define	AD7091R_CH_REG_CONV_CH0												((uint16_t)0x01)
	#define	AD7091R_CH_REG_CONV_CH1												((uint16_t)0x02)
	#define	AD7091R_CH_REG_CONV_CH2												((uint16_t)0x04)
	#define	AD7091R_CH_REG_CONV_CH3												((uint16_t)0x08)
	#define	AD7091R_CH_REG_CONV_CH4												((uint16_t)0x10)
	#define	AD7091R_CH_REG_CONV_CH5												((uint16_t)0x20)
	#define	AD7091R_CH_REG_CONV_CH6												((uint16_t)0x40)
	#define	AD7091R_CH_REG_CONV_CH7												((uint16_t)0x80)
	
	//CONFIGURATION REGISTER
	#define	AD7091R_CFG_REG_P_DOWN_MODE_0									((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_P_DOWN_MODE_1									((uint16_t)0x0001)
	#define	AD7091R_CFG_REG_P_DOWN_MODE_2									((uint16_t)0x0002)
	#define	AD7091R_CFG_REG_P_DOWN_MODE_3									((uint16_t)0x0003)
	
	#define	AD7091R_CFG_REG_GPO1_DRIVE_0									((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_GPO1_DRIVE_1									((uint16_t)0x0004)
	
	#define	AD7091R_CFG_REG_ALERT_LOW_ALERT								((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_ALERT_HIGH_ALERT							((uint16_t)0x0008)
	
	#define	AD7091R_CFG_REG_ALERT_GPO0										((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_ALERT_ALERT_BUSY							((uint16_t)0x0010)
	
	#define	AD7091R_CFG_REG_BUSY_DISABLE									((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_BUSY_ENABLE										((uint16_t)0x0020)
	
	#define	AD7091R_CFG_REG_ALERT_DRIVE_TYPE_OPEN_DRAIN		((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_ALERT_DRIVE_TYPE_CMOS					((uint16_t)0x0040)
	
	#define	AD7091R_CFG_REG_ALERT_STICKY_HYSTERESIS				((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_ALERT_STICKY_SOFT_RESET				((uint16_t)0x0080)
	
	#define	AD7091R_CFG_REG_SRST_NOT_ACTIVE								((uint16_t)0x0000)
	#define	AD7091R_CFG_REG_SRST_SOFT_RESET								((uint16_t)0x0200)


	/*!
		\brief Результат выполнения работы с чипом
	 */ 
	typedef enum
	{
		AD7091R_RESULT_OK,													///< Данные записаны/прочитаны корректно
		AD7091R_RESULT_SPI_FAILURE,									///< Ошибка на линии SPI
		AD7091R_RESULT_REG_WRONG_DATA_IS_WRITTEN,		///< В регистр записаны неверные данные
	}AD7091R_RESULT;


	/*!
		\brief Структура с SPI данными чипа
	 */ 
	typedef struct
	{
		MDR_SSP_TypeDef			*SSPx;				///< Указатель на модуль SPI
		MDR_PORT_TypeDef*		csPORTx;			///< Указатель на порт пина cs
		uint32_t						csPORT_Pin;		///< Пин cs
		uint16_t						spiFrame;			///< SPI кадр приёма/отправки
	}tSpiInfo;
	
	
	/*!
		\brief Структура с указателями на системные входы/выходы чипа
	 */ 
	typedef struct
	{
		MDR_PORT_TypeDef*		rstPORTx;
		uint32_t						rstPORT_Pin;
		MDR_PORT_TypeDef*		adcCnvPORTx;
		uint32_t						adcCnvPORT_Pin;
		MDR_PORT_TypeDef*		adcBusyPORTx;
		uint32_t						adcBusyPORT_Pin;
	}tPinsInfo;
	

	/*!
		\brief Структура с данными чипа - API
	 */ 
	typedef struct
	{
		uint8_t						chipRef;														///< Идентификатор чипа
		tSpiInfo					spiInfo;														///< SPI данные
		tPinsInfo					pinsInfo;														///< Системные входы/выходы
		uint8_t						chUsage;														///< 
		uint16_t					chVal[AD7091R_NUMBER_OF_CHANNELS];	///< 
	}AD7091R_API;
	
	
	extern AD7091R_API AD7091R_APIDefinitions[AD7091R_NUMBER_OF_CHIPS];
	
	
	// Прототипы функций
	void	AD7091R_init(uint8_t					API_ref,
									MDR_SSP_TypeDef			*SSPx,
									MDR_PORT_TypeDef*		csPORTx,
									uint32_t						csPORT_Pin,
									MDR_PORT_TypeDef*		rstPORTx,
									uint32_t						rstPORT_Pin,
									MDR_PORT_TypeDef*		adcCnvPORTx,
									uint32_t						adcCnvPORT_Pin,
									MDR_PORT_TypeDef*		adcRdyPORTx,
									uint32_t						adcRdyPORT_Pin);
	
	void	AD7091R_hwReset(uint8_t	API_ref);
	
	void	AD7091R_config(uint8_t	API_ref,
											uint16_t	P_DOWN,
											uint16_t	GPO1,
											uint16_t	ALERT_POL_OR_GPO0,
											uint16_t	ALERT_EN_OR_GPO0,
											uint16_t	BUSY,
											uint16_t	ALERT_DRIVE_TYPE,
											uint16_t	ALERT_STICKY,
											uint16_t	SRST);
	
	void	AD7091R_enChannel(uint8_t API_ref, uint16_t chMask);
	
	void	AD7091R_handler(void);
	
	uint16_t AD7091R_readReg(uint8_t API_ref, uint8_t nRegAdr);
	
	
#endif
