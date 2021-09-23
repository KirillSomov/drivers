
/*!
	\defgroup MCP23S17 Драйвер расширителя портов MCP23S17
	\details Драйвер для работы с расширителем портов входа/выхода MCP23S17
 */
///@{


#include "MCP23S17.h"


/*static*/ MCP23S17_API MCP23S17_APIDefinitions[NUMBER_OF_MCP23S17];


static inline MCP23S17_API *getPtrFromRef(uint8_t API_ref);

static void initialiseAPI(MCP23S17_API	*pAPI,
										uint8_t							API_ref,
										MDR_SSP_TypeDef			*SSPx,
										uint8_t							chipAddr,
										MDR_PORT_TypeDef*		csPORTx,
										uint32_t						csPORT_pin,
										MDR_PORT_TypeDef*		rstPORTx,
										uint32_t						rstPORT_pin);

static inline void SPI_setCS(MCP23S17_API	*pAPI);
static inline void SPI_resetCS(MCP23S17_API	*pAPI);

static MCP23S17_RESULT SPI_writeReg(MCP23S17_API	*pAPI,
																		uint8_t regAddr,
																		uint8_t data);


/*!
	\defgroup MCP23S17_SYSTEM_FUNCS Системные функции
	\ingroup MCP23S17
	\details Системные функции драйвера недоступные пользователю
 */
///@{
/*!
	\brief Получение указателя на структуру с данными чипа
	\param API_ref	Идентификатор реализации чипа
	\returns Указатель на структуру с данными чипа
 */
static inline MCP23S17_API *getPtrFromRef(uint8_t API_ref)
{
	return (((API_ref!=0) && (API_ref <= NUMBER_OF_MCP23S17))?(&MCP23S17_APIDefinitions[API_ref-1]):0);
}


/*!
	\brief Инициализация API чипа
	\param pAPI					Указатель на структуру данных чипа
	\param API_ref			Идентификатор реализации чипа
	\param SSPx					Указатель на модуль SPI
	\param chipAddr			Адрес чипа на SPI линии по адресным перемычкам
	\param csPORTx			Указатель на порт пина cs
	\param csPORT_pin		Пин cs SPI
	\param rstPORTx			Указатель на порт пина rst
	\param rstPORT_pin	Пин rst чипа
 */
static void initialiseAPI(MCP23S17_API	*pAPI,
										uint8_t							API_ref,
										MDR_SSP_TypeDef			*SSPx,
										uint8_t							chipAddr,
										MDR_PORT_TypeDef*		csPORTx,
										uint32_t						csPORT_pin,
										MDR_PORT_TypeDef*		rstPORTx,
										uint32_t						rstPORT_pin)
{
	if(pAPI)
	{
		memset(pAPI, 0, sizeof(MCP23S17_API));
		pAPI->chipRef 								= API_ref;
		pAPI->spiInfo.SSPx						=	SSPx;
		pAPI->spiInfo.chipAddr				=	chipAddr;
		pAPI->spiInfo.csPORTx					=	csPORTx;
		pAPI->spiInfo.csPORT_pin			=	csPORT_pin;
		pAPI->pinsInfo.rstPORTx				=	rstPORTx;
		pAPI->pinsInfo.rstPORT_pin		=	rstPORT_pin;
	}
}


/*!
	\brief Установить CS
	\param pAPI	Указатель на структуру данных чипа
 */
static inline void	SPI_setCS(MCP23S17_API *pAPI)
{
	PORT_SetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_pin);
}


/*!
	\brief Cбросить CS
	\param pAPI	Указатель на структуру данных чипа
 */
static inline void	SPI_resetCS(MCP23S17_API *pAPI)
{
	PORT_ResetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_pin);
}


/*!
	\brief Запись в регистр чипа
	\param pAPI			Указатель на структуру данных чипа
	\param regAddr	Адрес регистра
	\param data			Данные записываемые в данных регистр
 */
static MCP23S17_RESULT SPI_writeReg(MCP23S17_API	*pAPI,
																		uint8_t regAddr,
																		uint8_t data)
{	
	MCP23S17_RESULT	result		=	MCP23S17_RESULT_OK;
	MDR_SSP_TypeDef	*SSPx			=	pAPI->spiInfo.SSPx;
	uint8_t					chipAddr	=	pAPI->spiInfo.chipAddr;
	
	SPI_resetCS(pAPI);
	
	// Ожидание освобождения модуля SSPx
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_BSY, false) != SPI_RESULT_OK)
		result = MCP23S17_RESULT_SPI_FAILURE;
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_TFE, true) != SPI_RESULT_OK)
		result = MCP23S17_RESULT_SPI_FAILURE;
		
	// Передача адреса чипа и признака записи
	SSP_SendData(SSPx, chipAddr&0xFE);
	// Передача адреса регистра
	SSP_SendData(SSPx, regAddr);		
	// Запись данных в регистр
	SSP_SendData(SSPx, data);
		
	// Ожидание завершения передачи
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_BSY, false) != SPI_RESULT_OK)
		result = MCP23S17_RESULT_SPI_FAILURE;

	SPI_setCS(pAPI);
	
	return result;
}
///@}


/*!
	\defgroup MCP23S17_USER_FUNCS Пользовательские функции
	\ingroup MCP23S17
	\details Пользовательские фунцкии для работы с чипом
 */
///@{
/*!
	\brief Сброс чипа
 */ 
void	MCP23S17_hwReset(void)
{
	PORT_ResetBits(DO_MCP_RES_MCU_PORT, DO_MCP_RES_MCU_PIN);
	delay_ms(MDR_TIMER2, 10);
	PORT_SetBits(DO_MCP_RES_MCU_PORT, DO_MCP_RES_MCU_PIN);
	delay_ms(MDR_TIMER2, 50);
}


/*!
	\brief Инициализация чипа
	\param API_ref			Идентификатор реализации чипа
	\param SSPx					Указатель на модуль SPI
	\param chipAddr			Адрес чипа на SPI линии по адресным перемычкам
	\param csPORTx			Указатель на порт пина cs
	\param csPORT_pin		Пин cs SPI
	\param rstPORTx			Указатель на порт пина rst
	\param rstPORT_pin	Пин rst чипа
 */
void	MCP23S17_init(uint8_t					API_ref,
								MDR_SSP_TypeDef			*SSPx,
								uint8_t							chipAddr,
								MDR_PORT_TypeDef*		csPORTx,
								uint32_t						csPORT_pin,
								MDR_PORT_TypeDef*		rstPORTx,
								uint32_t						rstPORT_pin)
{
	MCP23S17_API *pAPI = getPtrFromRef(API_ref);
	uint8_t config = 0;
	
	if(pAPI)
	{
		initialiseAPI(pAPI,	API_ref,
									SSPx,	chipAddr,	csPORTx,	csPORT_pin,
									rstPORTx,	rstPORT_pin);
		
		config = MCP23S17_IOCON_BANK_SEQ		|	MCP23S17_IOCON_MIRROR_DIS
						|MCP23S17_IOCON_SEQOP_DIS		|	MCP23S17_IOCON_DISSLW_EN
						|MCP23S17_IOCON_HAEN_DIS		|	MCP23S17_IOCON_ODR_ACTIVE_DRIVER
						|MCP23S17_IOCON_INTPOL_LOW;
		
		SPI_writeReg(pAPI, MCP23S17_IOCONA_REG, config);
		SPI_writeReg(pAPI, MCP23S17_IODIRA_REG, 0x00);		// запись в регистр
		SPI_writeReg(pAPI, MCP23S17_IODIRB_REG, 0x00);		// запись в регистр
	}
}


/*!
	\brief Установка битов порта
	\param API_ref		Идентификатор реализации чипа
	\param PORT_Pin_x	Устанавливаемые биты
 */
void	MCP23S17_portSetBits(uint8_t	API_ref, uint32_t	PORT_Pin_x)
{
	MCP23S17_API *pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		pAPI->portBits.newValue |= PORT_Pin_x;
	}
}


/*!
	\brief Сброс битов порта
	\param API_ref		Идентификатор реализации чипа
	\param PORT_Pin_x	Сбрасываемые биты
 */
void	MCP23S17_portResetBits(uint8_t	API_ref, uint32_t	PORT_Pin_x)
{
	MCP23S17_API *pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		pAPI->portBits.newValue &= ~PORT_Pin_x;
	}
}


/*!
	\brief Запись битов порта
	\param API_ref	Идентификатор реализации чипа
 */
void	MCP23S17_portCommit(uint8_t	API_ref)
{
	MCP23S17_API *pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		if((pAPI->portBits.prevValue & 0x00FF) != (pAPI->portBits.newValue & 0x00FF))
		{
			SPI_writeReg(pAPI, MCP23S17_GPIOA_REG, (uint8_t)(pAPI->portBits.newValue & 0x00FF));
		}
		if((pAPI->portBits.prevValue & 0xFF00) != (pAPI->portBits.newValue & 0xFF00))
		{
			SPI_writeReg(pAPI, MCP23S17_GPIOB_REG, (uint8_t)((pAPI->portBits.newValue & 0xFF00) >> 8));
		}
		
		pAPI->portBits.prevValue = pAPI->portBits.newValue;
	}
}
///@}
///@}
