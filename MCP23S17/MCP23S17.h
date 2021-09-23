
#ifndef MCP23S17_H
	#define MCP23S17_H

	// Подключение заголовочных файлов модулей проекта
	#include "link.h"
	
	
	// SPI адрес чипа
	#define	MCP23S17_SPI						MDR_SSP1
	#define	MCP23S17_SPI_ADDR				((uint8_t)0x40)
	#define	MCP23S17_SPI_ADDR_0			((uint8_t)(MCP23S17_SPI_ADDR|0x00))
	#define	MCP23S17_SPI_ADDR_1			((uint8_t)(MCP23S17_SPI_ADDR|0x02))
	#define	MCP23S17_SPI_ADDR_2			((uint8_t)(MCP23S17_SPI_ADDR|0x04))
	#define	MCP23S17_SPI_ADDR_3			((uint8_t)(MCP23S17_SPI_ADDR|0x08))
	
	// Адреса регистров	
	#define	MCP23S17_IODIRA_REG			((uint8_t)0x00)
	#define MCP23S17_IPOLA_REG			((uint8_t)0x02)
	#define MCP23S17_GPINTENA_REG		((uint8_t)0x04)
	#define MCP23S17_DEFVALA_REG		((uint8_t)0x06)
	#define MCP23S17_INTCONA_REG		((uint8_t)0x08)
	#define MCP23S17_IOCONA_REG			((uint8_t)0x0A)
	#define MCP23S17_GPPUA_REG			((uint8_t)0x0C)
	#define MCP23S17_INTFA_REG			((uint8_t)0x0E)
	#define MCP23S17_INTCAPA_REG		((uint8_t)0x10)
	#define MCP23S17_GPIOA_REG			((uint8_t)0x12)
	#define MCP23S17_OLATA_REG			((uint8_t)0x14)
	
	#define	MCP23S17_IODIRB_REG			((uint8_t)0x01)
	#define MCP23S17_IPOLB_REG			((uint8_t)0x03)
	#define MCP23S17_GPINTENB_REG		((uint8_t)0x05)
	#define MCP23S17_DEFVALB_REG		((uint8_t)0x07)
	#define MCP23S17_INTCONB_REG		((uint8_t)0x09)
	#define MCP23S17_IOCONB_REG			((uint8_t)0x0B)
	#define MCP23S17_GPPUB_REG			((uint8_t)0x0D)
	#define MCP23S17_INTFB_REG			((uint8_t)0x0F)
	#define MCP23S17_INTCAPB_REG		((uint8_t)0x11)
	#define MCP23S17_GPIOB_REG			((uint8_t)0x13)
	#define MCP23S17_OLATB_REG			((uint8_t)0x15)
	
	
	// Значения регистров
	#define MCP23S17_IOCON_BANK_SEP						((uint8_t)0x80)
	#define MCP23S17_IOCON_BANK_SEQ						((uint8_t)0x00)
	#define MCP23S17_IOCON_MIRROR_EN					((uint8_t)0x40)
	#define MCP23S17_IOCON_MIRROR_DIS					((uint8_t)0x00)
	#define MCP23S17_IOCON_SEQOP_DIS					((uint8_t)0x20)
	#define MCP23S17_IOCON_SEQOP_EN						((uint8_t)0x00)
	#define MCP23S17_IOCON_DISSLW_DIS					((uint8_t)0x10)
	#define MCP23S17_IOCON_DISSLW_EN					((uint8_t)0x00)
	#define MCP23S17_IOCON_HAEN_EN						((uint8_t)0x08)
	#define MCP23S17_IOCON_HAEN_DIS						((uint8_t)0x00)
	#define MCP23S17_IOCON_ODR_OPEN_DRAIN			((uint8_t)0x04)
	#define MCP23S17_IOCON_ODR_ACTIVE_DRIVER	((uint8_t)0x00)
	#define MCP23S17_IOCON_INTPOL_HIGH				((uint8_t)0x02)
	#define MCP23S17_IOCON_INTPOL_LOW					((uint8_t)0x00)
	
	
	#define NUMBER_OF_MCP23S17		4
	
		
	/*!
		\brief Список возможных ошибок в работе чипа
	 */ 
	typedef enum
	{
		MCP23S17_RESULT_OK					=	0,	///< Операция выполнена успешно
		MCP23S17_RESULT_SPI_FAILURE	=	1,
	}MCP23S17_RESULT;
	

	/*!
		\brief Структура с SPI данными чипа
	 */ 
	typedef struct
	{
		MDR_SSP_TypeDef			*SSPx;				///< Указатель на модуль SPI
		MDR_PORT_TypeDef*		csPORTx;			///< Указатель на порт пина cs
		uint32_t						csPORT_pin;		///< Пин cs
		uint8_t							chipAddr;			///< Адрес чипа на SPI линии по адресным перемычкам
	}MCP23S17_spiInfo;
	
	
	/*!
		\brief Структура с указателями на системные входы/выходы чипа
	 */ 
	typedef struct
	{
		MDR_PORT_TypeDef*		rstPORTx;			///< Указатель на порт пина rst
		uint32_t						rstPORT_pin;	///< Пин rst чипа
	}MCP23S17_pinsInfo;


	/*!
		\brief Структура с портами входов/выходов чипа
	 */ 
	typedef struct
	{
		uint16_t	prevValue;	///< Предыдущее значение порта
		uint16_t	newValue;		///< Новое значение порта
	}MCP23S17_port;
		
	
	/*!
		\brief Структура с данными чипа - API
	 */ 
	typedef struct
	{
		uint8_t							chipRef;			///< Идентификатор чипа
		MCP23S17_spiInfo		spiInfo;			///< SPI данные
		MCP23S17_pinsInfo		pinsInfo;			///< Системные входы/выходы
		MCP23S17_port				portBits;			///< Порты ввода/вывода
	}MCP23S17_API;

	extern MCP23S17_API MCP23S17_APIDefinitions[NUMBER_OF_MCP23S17];
	
	// Прототипы функций
	void	MCP23S17_hwReset(void);
	
	void	MCP23S17_init(uint8_t				API_ref,
								MDR_SSP_TypeDef			*SSPx,
								uint8_t							chipAddr,
								MDR_PORT_TypeDef*		csPORTx,
								uint32_t						csPORT_pin,
								MDR_PORT_TypeDef*		rstPORTx,
								uint32_t						rstPORT_pin);
	
	void	MCP23S17_portSetBits(uint8_t	API_ref, uint32_t	PORT_Pin_x);
	void	MCP23S17_portResetBits(uint8_t	API_ref, uint32_t	PORT_Pin_x);
	void	MCP23S17_portCommit(uint8_t	API_ref);
	
	
#endif
