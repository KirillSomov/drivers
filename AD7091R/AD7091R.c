
/*!
	\defgroup AD7091R Драйвер АЦП AD7091R
	\details Драйвер для работы с АЦП AD7091R
 */
///@{

#include "AD7091R.h"


/*static*/ AD7091R_API AD7091R_APIDefinitions[AD7091R_NUMBER_OF_CHIPS];


static inline AD7091R_API *getPtrFromRef(uint8_t API_ref);

static void initialiseAPI(AD7091R_API		*pAPI,
										uint8_t							API_ref,
										MDR_SSP_TypeDef			*SSPx,
										MDR_PORT_TypeDef*		csPORTx,
										uint32_t						csPORT_Pin,
										MDR_PORT_TypeDef*		rstPORTx,
										uint32_t						rstPORT_Pin,
										MDR_PORT_TypeDef*		adcCnvPORTx,
										uint32_t						adcCnvPORT_Pin,
										MDR_PORT_TypeDef*		adcBusyPORTx,
										uint32_t						adcBusyPORT_Pin);

static inline void SPI_setCS(AD7091R_API	*pAPI);
static inline void SPI_resetCS(AD7091R_API	*pAPI);

static inline void createSpiFrame(AD7091R_API	*pAPI,
																	uint16_t RW_CMD,
																	uint16_t nRegAdr, uint16_t nData);
static inline uint16_t getSpiFrame(AD7091R_API	*pAPI);

static AD7091R_RESULT	SPI_writeFrame16(AD7091R_API	*pAPI,
															uint16_t RW_CMD,
															uint16_t nRegAdr, uint16_t nData,
															bool		validate);
static AD7091R_RESULT	SPI_writeNOP(AD7091R_API *pAPI);
static AD7091R_RESULT	SPI_readFrame16(AD7091R_API	*pAPI,
																							uint8_t	nRegAdr);

static inline void initConv(AD7091R_API	*pAPI);
static inline bool isAdcConvReady(AD7091R_API	*pAPI);

static void getAdcRes(AD7091R_API *pAPI);


/*!
	\defgroup AD7091R_SYSTEM_FUNCS Системные функции
	\ingroup AD7091R
	\details Системные функции драйвера недоступные пользователю
 */
///@{
/*!
	\brief Получение указателя на структуру с данными чипа
	\param API_ref	Идентификатор реализации чипа
	\return Указатель на структуру с данными чипа
 */
static inline AD7091R_API *getPtrFromRef(uint8_t API_ref)
{
	return (((API_ref!=0) && (API_ref <= AD7091R_NUMBER_OF_CHIPS))?(&AD7091R_APIDefinitions[API_ref-1]):0);
}


/*!
	\brief Инициализация API чипа
	\param pAPI							Указатель на структуру данных чипа
	\param API_ref					Идентификатор реализации чипа
	\param SSPx							Указатель на модуль SPI
	\param csPORTx					Указатель на порт пина cs
	\param csPORT_pin				Пин cs SPI
	\param rstPORTx					Указатель на порт пина rst
	\param rstPORT_pin			Пин rst чипа
	\param adcCnvPORTx			Указатель на порт пина cnv начала конвертации
	\param adcCnvPORT_Pin		Пин cnv
	\param adcBusyPORTx			Указатель на порт пина busy занятности чипа
	\param adcBusyPORT_Pin	Пин busy
 */
static void initialiseAPI(AD7091R_API					*pAPI,
													uint8_t							API_ref,
													MDR_SSP_TypeDef			*SSPx,
													MDR_PORT_TypeDef*		csPORTx,
													uint32_t						csPORT_Pin,
													MDR_PORT_TypeDef*		rstPORTx,
													uint32_t						rstPORT_Pin,
													MDR_PORT_TypeDef*		adcCnvPORTx,
													uint32_t						adcCnvPORT_Pin,
													MDR_PORT_TypeDef*		adcBusyPORTx,
													uint32_t						adcBusyPORT_Pin)
{
	if(pAPI)
	{
		memset(pAPI, 0, sizeof(AD7091R_API));
		pAPI->chipRef 									= API_ref;
		pAPI->spiInfo.SSPx							=	SSPx;
		pAPI->spiInfo.csPORTx						=	csPORTx;
		pAPI->spiInfo.csPORT_Pin				=	csPORT_Pin;
		pAPI->pinsInfo.rstPORTx					=	rstPORTx;
		pAPI->pinsInfo.rstPORT_Pin			=	rstPORT_Pin;
		pAPI->pinsInfo.adcCnvPORTx			=	adcCnvPORTx;
		pAPI->pinsInfo.adcCnvPORT_Pin		=	adcCnvPORT_Pin;
		pAPI->pinsInfo.adcBusyPORTx			=	adcBusyPORTx;
		pAPI->pinsInfo.adcBusyPORT_Pin	=	adcBusyPORT_Pin;
	}
}


/*!
	\brief Установить CS
	\param pAPI	Указатель на структуру данных чипа
 */
static inline void SPI_setCS(AD7091R_API	*pAPI)
{
	PORT_SetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_Pin);
}

/*!
	\brief Сбросить CS
	\param pAPI	Указатель на структуру данных чипа
 */
static inline void SPI_resetCS(AD7091R_API	*pAPI)
{
	PORT_ResetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_Pin);
}


/*!
	\brief Создать кадр данных для передачи
	\param pAPI	Указатель на структуру данных чипа
	\param RW_CMD		Признак записи/чтения данных
	\param nRegAdr	Адрес регистра
	\param nData		Данные записываемые в регистр
 */
static inline void createSpiFrame(AD7091R_API	*pAPI,
																	uint16_t RW_CMD,
																	uint16_t nRegAdr, uint16_t nData)
{
	pAPI->spiInfo.spiFrame = (nData & 0x3FF)
														|RW_CMD
														|((nRegAdr << 11) & 0xF800);
}

/*!
	\brief Получить кадр SPI
	\param pAPI	Указатель на структуру данных чипа
	\return Кадр SPI
 */
static inline uint16_t getSpiFrame(AD7091R_API	*pAPI)
{
	return pAPI->spiInfo.spiFrame;
}

/*!
	\brief Получить поле data кадра SPI
	\param pAPI	Указатель на структуру данных чипа
	\return Поле data кадра SPI
 */
static inline uint16_t getSpiFrameData(AD7091R_API	*pAPI)
{
	return pAPI->spiInfo.spiFrame & 0x0FFF;
}


/*!
	\brief Запись в регистр чипа
	\param pAPI	Указатель на структуру данных чипа
	\param RW_CMD		Признак записи/чтения данных
	\param nRegAdr	Адрес регистра
	\param nData		Данные записываемые в регистр
	\param validate	Проверка записанных данных чтением
 */
static AD7091R_RESULT	SPI_writeFrame16(AD7091R_API	*pAPI,
																				uint16_t RW_CMD,
																				uint16_t nRegAdr, uint16_t nData,
																				bool	validate)
{	
	AD7091R_RESULT	result					=	AD7091R_RESULT_OK;
	uint16_t				readBackData		= 0;
	
	createSpiFrame(pAPI, RW_CMD, nRegAdr, nData);
		
	SPI_resetCS(pAPI);
	
	// ожидание освобождения модуля SSPx
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_BSY, false) != SPI_RESULT_OK)
		result = AD7091R_RESULT_SPI_FAILURE;
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_TFE, true) != SPI_RESULT_OK)
		result = AD7091R_RESULT_SPI_FAILURE;
	
	// передача кадра
	SSP_SendData(pAPI->spiInfo.SSPx, pAPI->spiInfo.spiFrame);
		
	// ожидание завершения передачи
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_BSY, false) != SPI_RESULT_OK)
		result = AD7091R_RESULT_SPI_FAILURE;
		
	SPI_setCS(pAPI);
		
	if(validate && (result == AD7091R_RESULT_OK))
	{
		result = SPI_readFrame16(pAPI, nRegAdr);
		if(result == AD7091R_RESULT_OK)
		{
			readBackData = getSpiFrameData(pAPI);
			if(readBackData != nData)
				result = AD7091R_RESULT_REG_WRONG_DATA_IS_WRITTEN;
		}
	}
	
	return	result;
}

/*!
	\brief Отправка пустого кадра
	\param pAPI	Указатель на структуру данных чипа
 */
static AD7091R_RESULT SPI_writeNOP(AD7091R_API *pAPI)
{
	return SPI_writeFrame16(pAPI, AD7091R_SPI_WRITE_CMD, AD7091R_CONVERSION_RESULT_REG, 0x0000, false);
}

/*!
	\brief Чтение регистра чипа
	\param pAPI			Указатель на структуру данных чипа
	\param nRegAdr	Адрес регистра
 */
static AD7091R_RESULT SPI_readFrame16(AD7091R_API	*pAPI,
																				uint8_t	nRegAdr)
{
	AD7091R_RESULT	result			=	AD7091R_RESULT_OK;
	uint16_t				startTick		=	0;
	uint16_t				spiFrame		=	0;
	
	result = SPI_writeFrame16(pAPI, AD7091R_SPI_READ_CMD, nRegAdr, 0, false);
	result = SPI_writeNOP(pAPI);
	
	startTick = TIMEOUT_TIMER->CNT;
	
	// флаг освобождения буфера передатчика
	while(SSP_GetFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_RNE))
	{
		spiFrame = SSP_ReceiveData(pAPI->spiInfo.SSPx);
		if(((uint16_t)((uint16_t)TIMEOUT_TIMER->CNT - startTick)) > TIMEOUT_TICKS)
		{
			result = AD7091R_RESULT_SPI_FAILURE;
			break;
		}
	}
	
	pAPI->spiInfo.spiFrame = spiFrame;
	
	return result;
}


/*!
	\brief Сигнал начала конвертации данных
	\param pAPI	Указатель на структуру данных чипа
 */
static inline void initConv(AD7091R_API	*pAPI)
{
	PORT_ResetBits(pAPI->pinsInfo.adcCnvPORTx, pAPI->pinsInfo.adcCnvPORT_Pin);
	PORT_SetBits(pAPI->pinsInfo.adcCnvPORTx, pAPI->pinsInfo.adcCnvPORT_Pin);
}

/*!
	\brief Проверка завершения конвертирования данных
	\param pAPI	Указатель на структуру данных чипа
 */
static inline bool isAdcConvReady(AD7091R_API	*pAPI)
{
	return (bool)PORT_ReadInputDataBit(pAPI->pinsInfo.adcBusyPORTx,
																			pAPI->pinsInfo.adcBusyPORT_Pin);
}


/*!
	\brief Получить данные измерений
	\details Функция читает данные измерений АЦП по всем каналам
						и кладёт их в соответсвующие поля структуры чипа (chVal)
	\param pAPI	Указатель на структуру данных чипа
 */
static void getAdcRes(AD7091R_API *pAPI)
{
	AD7091R_RESULT	result	=	AD7091R_RESULT_OK;
	uint16_t convResult = 0;
	uint8_t chId = 0;
	uint8_t chAdcConvReady = 0;
		
	while(chAdcConvReady != pAPI->chUsage)
	{
		initConv(pAPI);
		//while(isAdcConvReady(pAPI));
		result = SPI_readFrame16(pAPI, AD7091R_CONVERSION_RESULT_REG);
		chId = (uint8_t)((getSpiFrame(pAPI) >> 13) & 0x0007);
		convResult = getSpiFrameData(pAPI);
		chAdcConvReady |= 1 << chId;
		pAPI->chVal[chId] = convResult;
	}
}
///@}


/*!
	\defgroup AD7091R_USER_FUNCS Пользовательские функции
	\ingroup AD7091R
	\details Пользовательские фунцкии для работы с чипом
 */
///@{
/*!
	\brief Инициализация чипа
	\param API_ref					Идентификатор реализации чипа
	\param SSPx							Указатель на модуль SPI
	\param csPORTx					Указатель на порт пина cs
	\param csPORT_pin				Пин cs SPI
	\param rstPORTx					Указатель на порт пина rst
	\param rstPORT_pin			Пин rst чипа
	\param adcCnvPORTx			Указатель на порт пина cnv начала конвертации
	\param adcCnvPORT_Pin		Пин cnv
	\param adcBusyPORTx			Указатель на порт пина busy занятности чипа
	\param adcBusyPORT_Pin	Пин busy
 */
void	AD7091R_init(uint8_t					API_ref,
								MDR_SSP_TypeDef			*SSPx,
								MDR_PORT_TypeDef*		csPORTx,
								uint32_t						csPORT_Pin,
								MDR_PORT_TypeDef*		rstPORTx,
								uint32_t						rstPORT_Pin,
								MDR_PORT_TypeDef*		adcCnvPORTx,
								uint32_t						adcCnvPORT_Pin,
								MDR_PORT_TypeDef*		adcRdyPORTx,
								uint32_t						adcRdyPORT_Pin)
{
	AD7091R_RESULT	result	=	AD7091R_RESULT_OK;
	AD7091R_API			*pAPI		=	getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		initialiseAPI(pAPI,	API_ref,
									SSPx,	csPORTx,	csPORT_Pin,
									rstPORTx,	rstPORT_Pin,
									adcCnvPORTx,	adcCnvPORT_Pin,
									adcRdyPORTx,	adcRdyPORT_Pin);
	}
}


/*!
	\brief Сброс чипа
 */
void	AD7091R_hwReset(uint8_t	API_ref)
{
	AD7091R_API *pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{	
		delay_ms(MDR_TIMER2, 20);
		PORT_ResetBits(pAPI->pinsInfo.rstPORTx, pAPI->pinsInfo.rstPORT_Pin);
		delay_ms(MDR_TIMER2, 1);
		PORT_SetBits(pAPI->pinsInfo.rstPORTx, pAPI->pinsInfo.rstPORT_Pin);
		delay_ms(MDR_TIMER2, 50);
	}
}


/*!
	\brief Конфигурация чипа
	\param API_ref						Идентификатор реализации чипа
	\param P_DOWN							Power Down mode
	\param GPO1								Value at GPO 1
	\param ALERT_POL_OR_GPO0	Polarity of ALERT pin
	\param ALERT_EN_OR_GPO0		Enable ALERT pin or GPO0
	\param BUSY								ALERT	pin indicates
	\param ALERT_DRIVE_TYPE		Drive Type of ALERT pin
	\param ALERT_STICKY				ALERT BIT
	\param SRST								Software Reset bit
 */
void	AD7091R_config(uint8_t		API_ref,
											uint16_t	P_DOWN,
											uint16_t	GPO1,
											uint16_t	ALERT_POL_OR_GPO0,
											uint16_t	ALERT_EN_OR_GPO0,
											uint16_t	BUSY,
											uint16_t	ALERT_DRIVE_TYPE,
											uint16_t	ALERT_STICKY,
											uint16_t	SRST)
{
	AD7091R_RESULT	result	=	AD7091R_RESULT_OK;
	AD7091R_API			*pAPI		=	getPtrFromRef(API_ref);
	uint16_t	configData		=	0;
	
	if(pAPI)
	{
		configData = P_DOWN
								|GPO1
								|ALERT_POL_OR_GPO0
								|ALERT_EN_OR_GPO0
								|BUSY
								|ALERT_DRIVE_TYPE
								|ALERT_STICKY
								|SRST;
		
		result = SPI_writeFrame16(pAPI,
															AD7091R_SPI_WRITE_CMD,
															AD7091R_CONFIGURATION_REG, configData,
															true);
	}
	
	return result;
}


/*!
	\brief Управление каналами чипа
	\details Функция включает каналы по маске
	\param chMask	Маска каналов
 */
void	AD7091R_enChannel(uint8_t API_ref, uint16_t chMask)
{
	AD7091R_RESULT	result	=	AD7091R_RESULT_OK;
	AD7091R_API			*pAPI		=	getPtrFromRef(API_ref);	
	
	if(pAPI)
	{
		pAPI->chUsage = chMask;
		
		result = SPI_writeFrame16(pAPI,
															AD7091R_SPI_WRITE_CMD,
															AD7091R_CHANNEL_REG, pAPI->chUsage,
															true);
	}
	
	return result;
}


/*!
	\brief Обработчик чипов
 */
void AD7091R_handler(void)
{
	AD7091R_API *pAPI = getPtrFromRef(1);
	getAdcRes(pAPI);
}


/*!
	\brief Прочитать данные регистра
	\param API_ref	Идентификатор реализации чипа
	\param nRegAdr	Адрес регистра
 */
uint16_t AD7091R_readReg(uint8_t API_ref, uint8_t nRegAdr)
{
	AD7091R_API *pAPI = getPtrFromRef(API_ref);
	(void)SPI_readFrame16(pAPI, nRegAdr);
	return getSpiFrame(pAPI);
}
///@}
///@}
