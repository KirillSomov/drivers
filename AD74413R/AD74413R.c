
#include "AD74413R.h"


/*static*/ AD74413R_API APIDefinitions[MAX_SUPPORTED_AD74413R];


static inline AD74413R_API *getPtrFromRef(uint8_t API_ref);

static inline uint8_t		BA_getAddr(uint8_t *BA_array);
static inline uint16_t	BA_getData(uint8_t *BA_array);
static inline uint8_t		BA_getCRC(uint8_t *BA_array);

static void initialiseAPI(AD74413R_API	*pAPI,
										uint8_t							API_ref,
										MDR_SSP_TypeDef			*SSPx,
										MDR_PORT_TypeDef*		csPORTx,
										uint32_t						csPORT_Pin,
										MDR_PORT_TypeDef*		rstPORTx,
										uint32_t						rstPORT_Pin,
										MDR_PORT_TypeDef*		adcRdyPORTx,
										uint32_t						adcRdyPORT_Pin);

static bool BA_CRC(uint8_t *pFrameBA, bool bUpdate);
static bool BA_validate(uint8_t *pFrameBA, uint8_t nRegAdr);
static void BA_create(uint8_t *pFrameBA, uint8_t nRegAdr, uint16_t nData);
static bool BA_extract(uint8_t *pFrameBA,	uint8_t nRegAdr,
											uint16_t	spiMsbFramePart,	uint16_t	spiLsbFramePart);

static inline void SPI_setCS(AD74413R_API	*pAPI);
static inline void SPI_resetCS(AD74413R_API	*pAPI);

static AD74413R_RESULT	SPI_writeFrame32(AD74413R_API	*pAPI,
																uint8_t nRegAdr, uint16_t nData,
																	bool	validate);
static AD74413R_RESULT	SPI_writeNOP(AD74413R_API *pAPI);
static AD74413R_RESULT	SPI_readFrame32(AD74413R_API	*pAPI,
																							uint8_t	nRegAdr);

static void	SPI_softwareReset(AD74413R_API	*pAPI);

static void setChState(AD74413R_API	*pAPI,
														uint8_t		chId,
																bool	chUsage);
static void setDiagState(AD74413R_API	*pAPI,
														uint8_t		diagId,
																bool	diagState);

static uint16_t calcDacCodeForVoltage(float	Volt);
static uint16_t calcDacCodeForCurrent(float	mA);

static void checkAlert(AD74413R_API *pAPI);
static void checkAdcDiag(AD74413R_API *pAPI);

static inline float calcCurrentInVoltageOutputMode(uint16_t	ADC_CODE,
																											float	Vmin,
																											float	Vrange);
static inline float calcVoltageInCurrentOutputMode(uint16_t	ADC_CODE,
																											float	Vrange);
static inline float calcVoltageInVoltageInputMode(uint16_t	ADC_CODE,
																										float		Vmin,
																										float		Vrange);
static inline float calcCurrentInCurrentInputMode(uint16_t	ADC_CODE,
																										float		Vrange);
static inline float calcResistanceInResMeasMode(uint16_t	ADC_CODE,
																									float		wireRes);

static void calcAdcRes(AD74413R_API *pAPI);

static float calcTemperature(uint16_t	DIAG_CODE);

static void calcDiagRes(AD74413R_API *pAPI);


static inline AD74413R_API *getPtrFromRef(uint8_t API_ref)
{
	return (((API_ref!=0) && (API_ref <= MAX_SUPPORTED_AD74413R))?(&APIDefinitions[API_ref-1]):0);
}


static inline uint8_t BA_getAddr(uint8_t *BA_array)
{
	return (uint8_t)(*(BA_array+0) & 0x7F);
}

static inline uint16_t BA_getData(uint8_t *BA_array)
{
	return (uint16_t)((((uint16_t)*(BA_array+1)) << 8) | (uint16_t)*(BA_array+2));
}

static inline uint8_t BA_getCRC(uint8_t *BA_array)
{
	return (uint8_t)*(BA_array+3);
}


//
static void initialiseAPI(AD74413R_API	*pAPI,
										uint8_t							API_ref,
										MDR_SSP_TypeDef			*SSPx,
										MDR_PORT_TypeDef*		csPORTx,
										uint32_t						csPORT_Pin,
										MDR_PORT_TypeDef*		rstPORTx,
										uint32_t						rstPORT_Pin,
										MDR_PORT_TypeDef*		adcRdyPORTx,
										uint32_t						adcRdyPORT_Pin)
{
	if(pAPI)
	{
		memset(pAPI, 0, sizeof(AD74413R_API));
		pAPI->chipRef 								= API_ref;
		pAPI->spiInfo.SSPx						=	SSPx;
		pAPI->spiInfo.csPORTx					=	csPORTx;
		pAPI->spiInfo.csPORT_Pin			=	csPORT_Pin;
		pAPI->pinsInfo.rstPORTx				=	rstPORTx;
		pAPI->pinsInfo.rstPORT_Pin		=	rstPORT_Pin;
		pAPI->pinsInfo.adcRdyPORTx		=	adcRdyPORTx;
		pAPI->pinsInfo.adcRdyPORT_Pin	=	adcRdyPORT_Pin;
		
		for(uint8_t chId = 0; chId < AD74413R_NUMBER_OF_CHANNELS; chId++)
		{
			pAPI->chInfo[chId].chMode = AD74413R_HIGH_IMPEDANCE;
		}
	}
}


/*!
 * @brief Calculate the CRC of a 4 bytes frame, and either update it or check it.

               To verify that data has been received
               correctly in noisy environ-ments, the
               ad74413R has a CRC implemented in the
               SPI interface.  This CRC is based on an
               8 bit cyclic redundancy check (CRC-8).
               The device controlling the ad74413R
               generates an 8-bit frame check sequence
               using the following polynomial:
               C(x) = x8 + x2 + x1 + 1
               This sequence is added to the end of
               the data-word, and 32 bits are sent
               to the ad74413R before taking SYNC high.

               E.g. 0x41002E has a CRC of 0x27

               E.g. 0xAEA020 has a CRC of 0x9C
   @param[in]   pFrameBA : Array of bytes to check/update.
   @param[in]   bUpdate  : True if to be updated, False if just to be checked.

  @return     False, if CRC is not valid after call, otherwise True
 */
static bool BA_CRC(uint8_t *pFrameBA, bool bUpdate)
{
	uint32_t CRCcode = 0x107;
	uint32_t i = 31;
	uint32_t DataOUT = (((uint32_t)pFrameBA[0]) << 24) |
										(((uint32_t)pFrameBA[1]) << 16) | (((uint32_t)pFrameBA[2]) << 8);
	
	while (i >= 8)
	{
			if ((DataOUT & (1 << i)) != 0)
			{
					DataOUT ^= (CRCcode << (i - 8));
			}
			i--;
	}
	
	if(bUpdate)
	{
			pFrameBA[3] = (uint8_t)DataOUT;
			return true;
	}
	else
	{
			if(pFrameBA[3] == (uint8_t)DataOUT)
					return true;
			else
					return false;
	}
}

/**
 *  \brief Check that the response word via that we received is valid
 *
 *  \param [in] nAdr         The address of the register than is expected here
 *  \param [in] pFrameBA     The 4 byte array as received from the device
 *
 *  \return Return_Description
 *
 *  \details Details
 */
static bool BA_validate(uint8_t *pFrameBA, uint8_t nRegAdr)
{
	bool bValid = true;
	
	// SPI_RD_RET_INFO = 0 !!
	// D31        1
	if ((pFrameBA[0] & 0x80) == 0x00)
			bValid = false;
	// D30:D24    READBACK_ADDR[6:0]
	if (BA_getAddr(pFrameBA) != (nRegAdr & 0x7F))
			bValid = false;
	
	// D23:D8     READ DATA
	//Result = (ResponseWord >> 8) & 0xFFFF;
	// D0:D7      CRC
	
	if(!BA_CRC(pFrameBA, false))
			bValid = false;
	
	return bValid;
}

/*!
 * @brief       Construct an array of bytes which are in the correct order for
                an SPI transaction representing a regster write
 *
   @param[in]   nAdr : Specifies the address of the register to write to.
   @param[in]   nData: Specifies the data to write to the register.
   @param[out]  pBytesTxOrdered:
  *
   @return
 */
static void BA_create(uint8_t *pFrameBA, uint8_t nRegAdr, uint16_t nData)
{
	// D31    : Reserved (will use 0)
	// D30:D24: Address.
	pFrameBA[0] = nRegAdr;
	// D23:D8 : Data (Big endian)
	pFrameBA[1] = (nData >> 8) & 0xFF;
	pFrameBA[2] = (nData >> 0) & 0xFF;
	// D7 :D0 : CRC
	
	/* Update the CRC of the Frame */
	(void)BA_CRC(pFrameBA, true);
}

//
static bool BA_extract(uint8_t *pFrameBA,	uint8_t nRegAdr,
											uint16_t	spiMsbFramePart,	uint16_t	spiLsbFramePart)
{
	bool bValid = true;
	
	// D31    : Reserved (will use 0)
	// D30:D24: Address.
	pFrameBA[0] = (uint8_t)((spiMsbFramePart >> 8) & 0xFF);
	// D23:D8 : Data (Big endian)
	pFrameBA[1] = (uint8_t)((spiMsbFramePart >> 0) & 0xFF);
	pFrameBA[2] = (uint8_t)((spiLsbFramePart >> 8) & 0xFF);
	// D7 :D0 : CRC
	pFrameBA[3] = (uint8_t)((spiLsbFramePart >> 0) & 0xFF);
	
	/* check CRC */
	bValid = BA_validate(pFrameBA, nRegAdr);
	
	return bValid;
}


//
static inline void SPI_setCS(AD74413R_API	*pAPI)
{
	//PORT_SetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_Pin);
	MCP23S17_portSetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_Pin);
	MCP23S17_portCommit(pAPI->spiInfo.csPORTx);
	delay_ms(MDR_TIMER2, 1);
}

//
static inline void SPI_resetCS(AD74413R_API	*pAPI)
{
	//PORT_ResetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_Pin);
	MCP23S17_portResetBits(pAPI->spiInfo.csPORTx, pAPI->spiInfo.csPORT_Pin);
	MCP23S17_portCommit(pAPI->spiInfo.csPORTx);
	delay_ms(MDR_TIMER2, 1);
}


// запись кадра в регистр
static AD74413R_RESULT	SPI_writeFrame32(AD74413R_API	*pAPI,
															uint8_t nRegAdr, uint16_t nData,
															bool		validate)
{	
	AD74413R_RESULT	result					=	AD74413R_RESULT_OK;
	uint16_t				readBackData		= 0;
	uint16_t				spiMsbFramePart =	0;
	uint16_t				spiLsbFramePart =	0;
	
	BA_create(pAPI->spiInfo.frameBA, nRegAdr, nData);
	
	spiMsbFramePart	=	((uint32_t)(pAPI->spiInfo.frameBA[0])) << 8
										|((uint32_t)(pAPI->spiInfo.frameBA[1])) << 0;
	spiLsbFramePart	=	((uint32_t)(pAPI->spiInfo.frameBA[2])) << 8
										|((uint32_t)(pAPI->spiInfo.frameBA[3])) << 0;
	
	SPI_resetCS(pAPI);
	
	// ожидание освобождения модуля SSPx
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_BSY, false) != SPI_RESULT_OK)
		result = AD74413R_RESULT_SPI_FAILURE;
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_TFE, true) != SPI_RESULT_OK)
		result = AD74413R_RESULT_SPI_FAILURE;
	
	// передача MSB и LSB
	SSP_SendData(pAPI->spiInfo.SSPx, spiMsbFramePart);
	SSP_SendData(pAPI->spiInfo.SSPx, spiLsbFramePart);
		
	// ожидание завершения передачи
	if(SPIx_getFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_BSY, false) != SPI_RESULT_OK)
		result = AD74413R_RESULT_SPI_FAILURE;
		
	SPI_setCS(pAPI);
		
	if(validate && (result == AD74413R_RESULT_OK))
	{
		result = SPI_readFrame32(pAPI, nRegAdr);
		if(result == AD74413R_RESULT_OK)
		{
			readBackData = BA_getData(pAPI->spiInfo.frameBA);
			if(readBackData != nData)
				result = AD74413R_RESULT_REG_WRONG_DATA_IS_WRITTEN;
		}
	}
	
	return	result;
}

//
static AD74413R_RESULT SPI_writeNOP(AD74413R_API *pAPI)
{
	return SPI_writeFrame32(pAPI, AD74413_REG_NOP, 0x0000, false);
}

// чтение регистра
static AD74413R_RESULT SPI_readFrame32(AD74413R_API	*pAPI,
																				uint8_t	nRegAdr)
{
	AD74413R_RESULT	result					=	AD74413R_RESULT_OK;
	bool						bValid					=	true;
	uint16_t				spiMsbFramePart =	0;
	uint16_t				spiLsbFramePart =	0;
	uint16_t				startTick				=	0;
	
	
	result = SPI_writeFrame32(pAPI, AD74413_REG_READ_SELECT, ((uint16_t)nRegAdr) | BITM_READ_SELECT_AUTO_RD_EN, false);
	result = SPI_writeNOP(pAPI);
	
	startTick = TIMEOUT_TIMER->CNT;
	
	// флаг освобождения буфера передатчика
	while(SSP_GetFlagStatus(pAPI->spiInfo.SSPx, SSP_FLAG_RNE))
	{
		spiMsbFramePart = SSP_ReceiveData(pAPI->spiInfo.SSPx);
		spiLsbFramePart = SSP_ReceiveData(pAPI->spiInfo.SSPx);
		if(((uint16_t)((uint16_t)TIMEOUT_TIMER->CNT - startTick)) > TIMEOUT_TICKS)
		{
			result = AD74413R_RESULT_SPI_FAILURE;
			break;
		}
	}
		
	if(result != AD74413R_RESULT_OK)
	{
		return result;
	}
	else
	{
		bValid = BA_extract(pAPI->spiInfo.frameBA, nRegAdr, spiMsbFramePart, spiLsbFramePart);
		
		if(bValid)
		{
			result = AD74413R_RESULT_OK;
			return result;
		}
		else
		{
			result = AD74413R_RESULT_CRC_FAILURE;
			return result;
		}
	}
}


//
static void	SPI_softwareReset(AD74413R_API	*pAPI)
{
	;
}


//
static void setChState(AD74413R_API	*pAPI,
														uint8_t		chId,
																bool	chUsage)
{	
	if(chUsage == true)
		pAPI->chUsage |= (1 << chId);
	else
		pAPI->chUsage &= ~(1 << chId);
}

//
static void setDiagState(AD74413R_API	*pAPI,
															uint8_t		diagId,
																	bool	diagUsage)
{	
	if(diagUsage == true)
		pAPI->chUsage |= (1 << diagId+4);
	else
		pAPI->chUsage &= ~(1 << diagId+4);
}


//
static uint16_t calcDacCodeForVoltage(float	Volt)
{
	uint16_t dacCode = 0;
	
	if(Volt >= VOLTAGE_OUTPUT_MIN && Volt <= VOLTAGE_OUTPUT_MAX)
	{
		dacCode = (uint16_t)(VOLTAGE_DAC_CODE_FOR_1V * Volt) & 0x1FFF;
	}
	
	return dacCode;
}	

//
static uint16_t calcDacCodeForCurrent(float	mA)
{
	uint16_t dacCode = 0;
	
	if(mA >= CURRENT_OUTPUT_MIN && mA <= CURRENT_OUTPUT_MAX)
	{
		dacCode = (uint16_t)(CURRENT_DAC_CODE_FOR_1MA * mA) & 0x1FFF;
	}
	
	return dacCode;
}


//
static void checkAlert(AD74413R_API *pAPI)
{
	uint16_t data = 0;

	(void)SPI_readFrame32(pAPI, AD74413_REG_ALERT_STATUS);
	data = BA_getData(pAPI->spiInfo.frameBA);
	
	*((uint16_t*)(&pAPI->alertInfo)) = data;
	
	
	(void)SPI_writeFrame32(pAPI, AD74413_REG_ALERT_STATUS,
												0xFFFF,
												false);
}

//
static void checkAdcDiag(AD74413R_API *pAPI)
{
	uint16_t data = 0;
	
	if(pAPI->chUsage != 0)
	{
		//while(PORT_ReadInputDataBit(pAPI->pinsInfo.adcRdyPORTx, pAPI->pinsInfo.adcRdyPORT_Pin))	{;}
			
		for(uint8_t chId = 0; chId < AD74413R_NUMBER_OF_ADC_CHANNELS; chId++)
		{
			if(pAPI->chUsage & (1 << chId))
			{
				(void)SPI_readFrame32(pAPI, AD74413_REG_ADC_RESULT0+chId);
				data = BA_getData(pAPI->spiInfo.frameBA);
				
				pAPI->chInfo[chId].adcCode = data;
			}
		}
		for(uint8_t diagId = 0; diagId < AD74413R_NUMBER_OF_DIAGNOSTICS; diagId++)
		{
			if(pAPI->chUsage & (1 << diagId+4))
			{
				(void)SPI_readFrame32(pAPI, AD74413_REG_DIAG_RESULT0+diagId);
				data = BA_getData(pAPI->spiInfo.frameBA);
				
				pAPI->diagInfo[diagId].diagCode = data;
			}
		}
	}
}


//
static inline  float calcCurrentInVoltageOutputMode(uint16_t	ADC_CODE,
																										float			Vmin,
																										float			Vrange)
{
	float current = 0.0f;
	
	current = ((Vmin + ((ADC_CODE/ADC_DIGIT) * Vrange)) / R_SENSE) * 1000.0f;
	
	return current;
}

//
static inline float calcVoltageInCurrentOutputMode(uint16_t	ADC_CODE,
																									float			Vrange)
{
	float voltage = 0.0f;
	
	voltage = (ADC_CODE/ADC_DIGIT) * Vrange;
	
	return voltage;
}

//
static inline float calcVoltageInVoltageInputMode(uint16_t	ADC_CODE,
																										float		Vmin,
																										float		Vrange)
{
	float voltage = 0.0f;
	
	voltage = Vmin + (ADC_CODE/ADC_DIGIT) * Vrange;
	
	return voltage;
}

//
static inline float calcCurrentInCurrentInputMode(uint16_t	ADC_CODE,
																										float		Vrange)
{
	float current = 0.0f;
	
	current = (((ADC_CODE/ADC_DIGIT) * Vrange) / R_SENSE) * 1000;
	
	return current;
}

//
static inline float calcResistanceInResMeasMode(uint16_t	ADC_CODE,
																									float		wireRes)
{
	float resistance = 0.0f;
		
	resistance = (ADC_CODE*R_PULL_UP) / (ADC_DIGIT-ADC_CODE) - wireRes;
	
	return resistance;
}


/* ========================= ACCURACY TEST ============================== */


static void clearMinMax(AD74413R_API *pAPI, uint8_t chId, float val)
{
	pAPI->chInfo[chId].accuracy.reload					=	false;
	pAPI->chInfo[chId].accuracy.averageCounter	=	0;
	pAPI->chInfo[chId].accuracy.averageVal			=	0;
	pAPI->chInfo[chId].accuracy.minVal					=	val;
	pAPI->chInfo[chId].accuracy.maxVal					=	0.0f;
	pAPI->chInfo[chId].accuracy.nDeviationPercentage	=	0.0f;
	pAPI->chInfo[chId].accuracy.pDeviationPercentage	=	0.0f;
}

static void checkMinMax(AD74413R_API *pAPI, uint8_t chId, float val)
{		
	if(pAPI->chInfo[chId].accuracy.reload == true)
	{
		clearMinMax(pAPI, chId, val);
	}
	
	if(pAPI->chInfo[chId].accuracy.averageCounter < pAPI->chInfo[chId].accuracy.averageCounts)
	{
		pAPI->chInfo[chId].accuracy.averageCounter++;
		
		pAPI->chInfo[chId].accuracy.averageVal += val;
		
		if(pAPI->chInfo[chId].accuracy.averageCounter == pAPI->chInfo[chId].accuracy.averageCounts)
		{
			pAPI->chInfo[chId].accuracy.averageVal = pAPI->chInfo[chId].accuracy.averageVal / pAPI->chInfo[chId].accuracy.averageCounts;
		}
		
		if(val < pAPI->chInfo[chId].accuracy.minVal)
			pAPI->chInfo[chId].accuracy.minVal = val;
		
		if(val > pAPI->chInfo[chId].accuracy.maxVal)
			pAPI->chInfo[chId].accuracy.maxVal = val;
		
		pAPI->chInfo[chId].accuracy.nDeviationPercentage	=	
					(1 - pAPI->chInfo[chId].accuracy.minVal / pAPI->chInfo[chId].accuracy.actualVal) * 100.0f;
		pAPI->chInfo[chId].accuracy.pDeviationPercentage	=	
					(pAPI->chInfo[chId].accuracy.maxVal / pAPI->chInfo[chId].accuracy.actualVal - 1) * 100.0f;
	}
}


/* ========================= ACCURACY TEST ============================== */


//
static void calcAdcRes(AD74413R_API *pAPI)
{
	AD74413R_CHANNEL_MODE chMode;
	uint16_t	adcCode	=	0;
	float			chVal		=	0.0f;
	float			wireRes	=	0.0f;
	
	for(uint8_t chId = 0; chId < AD74413R_NUMBER_OF_ADC_CHANNELS; chId++)
	{
		chMode	=	pAPI->chInfo[chId].chMode;
		adcCode	=	pAPI->chInfo[chId].adcCode;
		
		switch(chMode)
		{
			case AD74413R_HIGH_IMPEDANCE:
				chVal = 0.0f;
				break;
			
			case AD74413R_VOLTAGE_OUTPUT:
				chVal = calcCurrentInVoltageOutputMode(adcCode, V_MIN_0V, V_RNG_0_10V);
				break;
			
			case AD74413R_CURRENT_OUTPUT:
				chVal	=	calcVoltageInCurrentOutputMode(adcCode, V_RNG_0_10V);
				break;
			
			case AD74413R_VOLTAGE_MEASUREMENT:
				chVal	=	calcVoltageInVoltageInputMode(adcCode, V_MIN_0V, V_RNG_0_10V);
				break;
			
			case AD74413R_CURRENT_MEASUREMENT:
				chVal	=	calcCurrentInCurrentInputMode(adcCode, V_RNG_0_2P5V);
				break;
			
			case AD74413R_RESISTANCE_MEASUREMENT:
				wireRes	=	pAPI->chInfo[chId].wireRes;
				chVal		=	calcResistanceInResMeasMode(adcCode, wireRes);
				break;
			
			default:
				break;
		}
		
		pAPI->chInfo[chId].chVal = chVal;
		
		checkMinMax(pAPI, chId, chVal);
	}
}


//
static float calcTemperature(uint16_t	DIAG_CODE)
{
	float temperature = 0.0f;
	
	temperature = (DIAG_CODE - 2034.0f) / 8.95f - 40.0f;
	
	return temperature;
}


//
static float calcALDO5V(uint16_t	DIAG_CODE)
{
	float ALDO5V = 0.0f;
	
	ALDO5V = 7.0f * (DIAG_CODE / ADC_DIGIT) * V_RNG_0_2P5V;
	
	return ALDO5V;
}


//
static void calcDiagRes(AD74413R_API *pAPI)
{
	AD74413R_DIAGNOSTIC_MODE diagMode;
	uint16_t	diagCode	=	0;
	float			diagVal		=	0.0f;
	
	for(uint8_t diagId = 0; diagId < AD74413R_NUMBER_OF_DIAGNOSTICS; diagId++)
	{
		diagMode	=	pAPI->diagInfo[diagId].diagMode;
		diagCode	=	pAPI->diagInfo[diagId].diagCode;
		
		switch(diagMode)
		{
			case TEMPERATURE:
				diagVal = calcTemperature(diagCode);
				break;
			
			case ALDO5V:
				diagVal = calcALDO5V(diagCode);
				break;
			
			default:
				break;
		}
		
		pAPI->diagInfo[diagId].diagVal = diagVal;
	}
}


/* ==================== || ========== || ==================== */
/* ==================== || USER LAYER || ==================== */
/* ==================== \/ ========== \/ ==================== */


// сброс
void	AD74413R_hwReset(void)
{
	delay_ms(MDR_TIMER2, 20);
	PORT_ResetBits(DO_AD74413R_RST_PORT, DO_AD74413R_RST_PIN);
	delay_ms(MDR_TIMER2, 1);
	PORT_SetBits(DO_AD74413R_RST_PORT, DO_AD74413R_RST_PIN);
	delay_ms(MDR_TIMER2, 50);
}


//
void	AD74413R_init(uint8_t					API_ref,
								MDR_SSP_TypeDef			*SSPx,
								MDR_PORT_TypeDef*		csPORTx,
								uint32_t						csPORT_Pin,
								MDR_PORT_TypeDef*		rstPORTx,
								uint32_t						rstPORT_Pin,
								MDR_PORT_TypeDef*		adcRdyPORTx,
								uint32_t						adcRdyPORT_Pin)
{
	AD74413R_RESULT	result	=	AD74413R_RESULT_OK;
	AD74413R_API		*pAPI		=	getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		initialiseAPI(pAPI,	API_ref,
									SSPx,	csPORTx,	csPORT_Pin,
									rstPORTx,	rstPORT_Pin,
									adcRdyPORTx,	adcRdyPORT_Pin);
		
		SPI_softwareReset(pAPI);
		
		for(uint8_t chId = 0; chId < AD74413R_NUMBER_OF_ADC_CHANNELS; chId++)
		{
			result = SPI_writeFrame32(pAPI, AD74413_REG_ADC_CONFIG0+chId,
										ENUM_ADC_CONFIG_SPS_4K,
										true);
			result = SPI_writeFrame32(pAPI, AD74413_REG_DIN_CONFIG0+chId,
										BITM_DIN_CONFIG_COMPARATOR_EN,
										true);
			result = SPI_writeFrame32(pAPI, AD74413_REG_GPO_CONFIG0+chId,
										ENUM_GPO_CONFIG_SEL_GPDATA,
										true);
		}
	}
}


//
void	AD74413R_analyzeChipStatus(uint8_t	API_ref)
{
	AD74413R_API *pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		;
	}
}


//
void	AD74413R_turnChipInWork(uint8_t	API_ref,
																bool	inUse)
{
	AD74413R_API *pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		//pAPI->inUse = inUse;
		;
	}
}


//
void	AD74413R_setRegData(uint8_t		API_ref,
													uint8_t		nRegAdr,
													uint16_t	nRegData,
															bool	validate)
{
	//AD74413R_RESULT	result	=	AD74413R_RESULT_OK;
	AD74413R_API 	*pAPI 	= getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		(void)SPI_writeFrame32(pAPI, nRegAdr, nRegData, validate);
	}
}

//
uint16_t	AD74413R_getRegData(uint8_t		API_ref,
															uint8_t		nRegAdr)
{
	uint16_t				data		=	0;
	AD74413R_API 	*pAPI 	= getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		(void)SPI_readFrame32(pAPI, nRegAdr);
		data = BA_getData(pAPI->spiInfo.frameBA);
	}
	
	return data;
}


//
void	AD74413R_setChMode(uint8_t	API_ref,
												uint8_t		chId,
					AD74413R_CHANNEL_MODE		chMode)
{
	AD74413R_API	*pAPI	= getPtrFromRef(API_ref);
	
	pAPI->chInfo[chId].chMode = chMode;
	
	if(pAPI)
	{
		(void)SPI_writeFrame32(pAPI, AD74413_REG_ADC_CONV_CTRL,
														ENUM_ADC_CONV_CTRL_IDLE
														|pAPI->chUsage,
														true);
		(void)SPI_writeFrame32(pAPI, AD74413_REG_DAC_CODE0+chId,
														0x0000,
														true);
		(void)SPI_writeFrame32(pAPI, (AD74413_REG_CH_FUNC_SETUP0+chId),
														ENUM_CH_FUNC_SETUP_HIGH_IMP,
														true);
		
		switch(chMode)
		{
			case AD74413R_CHANNEL_OFF:			
			case AD74413R_HIGH_IMPEDANCE:
				setChState(pAPI, chId, false);
				if(pAPI->chUsage == 0)
				{
					(void)SPI_writeFrame32(pAPI, AD74413_REG_ADC_CONV_CTRL,
																	ENUM_ADC_CONV_CTRL_ADC_PWRDWN,
																	true);
				}
				break;
			
			case AD74413R_VOLTAGE_OUTPUT:
				(void)SPI_writeFrame32(pAPI, AD74413_REG_CH_FUNC_SETUP0+chId,
																ENUM_CH_FUNC_SETUP_VOUT,
																true);
				setChState(pAPI, chId, true);
				break;
			
			case AD74413R_CURRENT_OUTPUT:
				(void)SPI_writeFrame32(pAPI, AD74413_REG_CH_FUNC_SETUP0+chId,
																ENUM_CH_FUNC_SETUP_IOUT,
																true);
				setChState(pAPI, chId, true);
				break;
				
			case AD74413R_CURRENT_MEASUREMENT:
				(void)SPI_writeFrame32(pAPI, AD74413_REG_CH_FUNC_SETUP0+chId,
																ENUM_CH_FUNC_SETUP_IIN_EXT_PWR,
																true);
				(void)SPI_writeFrame32(pAPI, AD74413_REG_DIN_CONFIG0+chId,
																BITM_DIN_CONFIG_COMPARATOR_EN,
																true);
				setChState(pAPI, chId, true);
				break;
			
			case AD74413R_VOLTAGE_MEASUREMENT:
				(void)SPI_writeFrame32(pAPI, AD74413_REG_CH_FUNC_SETUP0+chId,
																ENUM_CH_FUNC_SETUP_VIN,
																true);
				(void)SPI_writeFrame32(pAPI, AD74413_REG_DIN_CONFIG0+chId,
																BITM_DIN_CONFIG_COMPARATOR_EN,
																true);
				setChState(pAPI, chId, true);
				break;
			
			case AD74413R_RESISTANCE_MEASUREMENT:
				(void)SPI_writeFrame32(pAPI, AD74413_REG_CH_FUNC_SETUP0+chId,
																ENUM_CH_FUNC_SETUP_RES_MEAS,
																true);
				(void)SPI_writeFrame32(pAPI, AD74413_REG_DIN_CONFIG0+chId,
																BITM_DIN_CONFIG_COMPARATOR_EN,
																true);
				setChState(pAPI, chId, true);
				break;
				
			default:
				setChState(pAPI, chId, false);
				break;
		}
		
		(void)SPI_writeFrame32(pAPI, AD74413_REG_ADC_CONV_CTRL,
													ENUM_ADC_CONV_CTRL_CONTINUOUS
													|pAPI->chUsage,
													true);
	}
}


//
AD74413R_RESULT	AD74413R_setOutputVoltageOnCh(uint8_t		API_ref,
																							uint8_t		chId,
																							float			volt)
{
	AD74413R_RESULT	result	=	AD74413R_RESULT_OK;
	AD74413R_API		*pAPI		=	NULL;
	uint16_t				dacCode = 0;
	
	if(volt >= VOLTAGE_OUTPUT_MIN && volt <= VOLTAGE_OUTPUT_MAX)
	{
		pAPI	= getPtrFromRef(API_ref);
		
		if(pAPI)
		{
			if(pAPI->chInfo[chId].chMode == AD74413R_VOLTAGE_OUTPUT)
			{
				dacCode = calcDacCodeForVoltage(volt);
				
				result = SPI_writeFrame32(pAPI, AD74413_REG_DAC_CODE0+chId,
																	dacCode,
																	true);
			}
		}
	}
	
	return result;
}

//
void	AD74413R_setOutputCurrentOnCh(uint8_t		API_ref,
																		uint8_t		chId,
																			float		mA)
{
	AD74413R_API	*pAPI;
	uint16_t			dacCode = 0;
	
	if(mA >= CURRENT_OUTPUT_MIN && mA <= CURRENT_OUTPUT_MAX)
	{
		pAPI	= getPtrFromRef(API_ref);
		
		if(pAPI)
		{
			if(pAPI->chInfo[chId].chMode == AD74413R_CURRENT_OUTPUT)
			{
				dacCode = calcDacCodeForCurrent(mA);
				
				(void)SPI_writeFrame32(pAPI, AD74413_REG_DAC_CODE0+chId,
																dacCode,
																true);
			}
		}
	}
}


//
void	AD74413R_setDiagMode(uint8_t	API_ref,
													uint8_t		diagId,
						AD74413R_DIAGNOSTIC_MODE	diagMode)
{
	AD74413R_API	*pAPI	= getPtrFromRef(API_ref);
	
	pAPI->diagInfo[diagId].diagMode = diagMode;
	
	if(pAPI)
	{				
		switch(diagMode)
		{
			case DIAG_OFF:
				setDiagState(pAPI, diagId, false);
				if(pAPI->chUsage == 0)
				{
					(void)SPI_writeFrame32(pAPI, AD74413_REG_ADC_CONV_CTRL,
																	ENUM_ADC_CONV_CTRL_ADC_PWRDWN,
																	true);
				}
				break;
				
			case TEMPERATURE:
				setDiagState(pAPI, diagId, true);
				(void)SPI_writeFrame32(pAPI, AD74413_REG_DIAG_ASSIGN,
																(ENUM_DIAG0_ASSIGN_TEMP << (diagId*15)),
																true);
				break;
				
			case ALDO5V:
				setDiagState(pAPI, diagId, true);
				(void)SPI_writeFrame32(pAPI, AD74413_REG_DIAG_ASSIGN,
																(ENUM_DIAG0_ASSIGN_ALDO5V << (diagId*15)),
																true);
				break;
				
			default:
				setDiagState(pAPI, diagId, false);
				break;
		}
				
		(void)SPI_writeFrame32(pAPI, AD74413_REG_ADC_CONV_CTRL,
													ENUM_ADC_CONV_CTRL_CONTINUOUS
													|pAPI->chUsage,
													true);
	}
}


float	AD74413R_getChValue(uint8_t		API_ref,
													uint8_t		chId)
{
	float	chValue	=	0;
	AD74413R_API	*pAPI	=	getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		chValue = pAPI->chInfo[chId].chVal;
	}
	
	return chValue;
}


float	AD74413R_getDiagValue(uint8_t		API_ref,
														uint8_t		diagId)
{
	float	diagValue	=	0;
	AD74413R_API	*pAPI	=	getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		diagValue = pAPI->diagInfo[diagId].diagVal;
	}
	
	return diagValue;
}


void	AD74413R_setGPO(uint8_t	API_ref, uint8_t chId)
{
	AD74413R_API	*pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		(void)SPI_writeFrame32(pAPI, AD74413_REG_GPO_CONFIG0+chId,
														ENUM_GPO_CONFIG_SEL_GPDATA
														|(0x01 << 3),
														true);
	}
}


void	AD74413R_resetGPO(uint8_t	API_ref, uint8_t chId)
{
	AD74413R_API	*pAPI = getPtrFromRef(API_ref);
	
	if(pAPI)
	{
		(void)SPI_writeFrame32(pAPI, AD74413_REG_GPO_CONFIG0+chId,
														ENUM_GPO_CONFIG_SEL_HIZ,
														true);
	}
}


//
void	AD74413R_handler(void)
{
	AD74413R_API	*pAPI;
	
	for(uint8_t apiRefNum = 0; apiRefNum < MAX_SUPPORTED_AD74413R; apiRefNum++)
	{
			pAPI	= getPtrFromRef(apiRefNum+1);
			if(pAPI)
			{
				checkAlert(pAPI);
				checkAdcDiag(pAPI);
				calcAdcRes(pAPI);
				calcDiagRes(pAPI);
			}
	}
}
