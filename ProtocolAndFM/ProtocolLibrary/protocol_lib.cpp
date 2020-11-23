//https://github.com/dwblair/arduino_crc8 DOWNLOAD THIS LIB FOR CRC8

#include <Arduino.h>
#include "protocol_lib.h"

ProtocolControl::ProtocolControl(String srcName, String destName, float freq)
{
	this->srcName = srcName;
	this->destName = destName;
	this->STARTFLAG = "o";
	this->allReceiving = "";
	this->ackNo = "0";
	this->rx = new FM_RX(freq);
	this->tx = new FM_TX();
	this->crc8 = new CRC8();
	Serial.println("Init completed");
}
ProtocolControl::~ProtocolControl()
{
}

String ProtocolControl::makeDataFrame(String textData, String frameNo, String ENDFLAG, String destName)
{
	/*
    User must shift the data manually!

    This code will generate a frame of 2 bytes/frame
    Will return a frame of data in this format
    START,dest,src,frameNo,data,data,Check,END

  */
	String toSend = ""; //Frame will be store here
	//Header
	toSend += STARTFLAG;
	toSend += destName;
	toSend += srcName;
	toSend += frameNo;

	//Body
	String data = textData;
	while (data.length() < 2) //Add padding: DIFF FROM OLD CODE
	{
		data += "~"; //type with ASCII 126
	}
	toSend += data;

	//Trailer: CRC8
	int str_len = toSend.length() + 1;
	unsigned char char_array[str_len];		 // prepare a character array (the buffer)
	toSend.toCharArray(char_array, str_len); // copy it over
	uint8_t checksum = crc8->get_crc8(char_array, str_len);

	toSend += char(checksum);

	toSend += ENDFLAG;

	return toSend;
}
bool ProtocolControl::approveDataFrame(String frame) //TODO: CHANGE TO CRC
{
	/*
    Analyze checker and return boolean true if the data is correct
  */
	if (frame.length() != 8)
	{
		return false;
	}

	if (frame[1] != this->srcName[0])
	{
		return false;
	}

	if (frame[3] != this->ackNo[0])
	{
		return false;
	}

	String toCheck = frame.substring(0, 6);
	int str_len = toCheck.length() + 1;		  // calculate length of message (with one extra character for the null terminator)
	unsigned char char_array[str_len];		  // prepare a character array (the buffer)
	toCheck.toCharArray(char_array, str_len); // copy it over
	uint8_t checksum = crc8->get_crc8(char_array, str_len);

	if (char(checksum) == frame[6])
	{
		return true;
	}
	else
	{
		return false;
	}
}

String ProtocolControl::makeAckFrame(String ackNo, String ENDFLAG, String destName)
{
	/*
    User must shift the data manually!

    This code will generate am acknowledge frame
    Will return a frame in this format
    START,dest,src,frameNo,Check,END
  */
	String toSend = ""; //Frame will be store here
	//Header
	toSend += STARTFLAG;
	toSend += destName;
	toSend += srcName;

	//Ack Number
	toSend += ackNo;

	//Trailer: CRC
	int str_len = toSend.length() + 1;
	unsigned char char_array[str_len];		 // prepare a character array (the buffer)
	toSend.toCharArray(char_array, str_len); // copy it over
	uint8_t checksum = crc8->get_crc8(char_array, str_len);
	toSend += char(checksum);

	toSend += ENDFLAG;

	return toSend;
}

bool ProtocolControl::approveAckFrame(String frame) //TODO: CHANGE TO CRC
{
	/*
    Analyze checker and return boolean true if the ack is correct
  */
	if (frame.length() != 6)
	{
		return false;
	}

	if (frame[1] != this->srcName[0])
	{
		return false;
	}

	String toCheck = frame.substring(0, 4);
	int str_len = toCheck.length() + 1;		  // calculate length of message (with one extra character for the null terminator)
	unsigned char char_array[str_len];		  // prepare a character array (the buffer)
	toCheck.toCharArray(char_array, str_len); // copy it over
	uint8_t checksum = crc8->get_crc8(char_array, str_len);

	if (char(checksum) == frame[4])
	{
		return true;
	}
	else
	{
		return false;
	}
}

void ProtocolControl::transmitter()
{
	if (Serial.available()) //Read data from serial
	{
		String frameNo = "0";
		String textData = "";
		const long TIMEOUT = 200;

		this->ackNo = "0";						 //reset ackNo
		this->allReceiving = "";				 //reset receiver
		textData = Serial.readStringUntil('\n'); //read data from serial
		//Serial.println(textData);

		while (textData.length() > 0) //Send All Data. Frame by Frame
		{
			//Make Data Frame
			String frame = "";
			if (textData.length() > 2)
				frame = this->makeDataFrame(textData.substring(0, 2), frameNo, "1", this->destName);
			else
				frame = this->makeDataFrame(textData.substring(0, 2), frameNo, "0", this->destName);

			while (true) //Send & "Wait"
			{
				for (int i = 0; i < frame.length(); i++) //Send
				{
					this->tx->sendFM(frame[i]);
				}
				long timer = millis();
				bool okAck = false;

				while (millis() - timer < TIMEOUT) //Wait for ACK
				{
					String ackFrame = this->rx->receiveStringFM(6); //receive ACK

					if (this->approveAckFrame(ackframe)) //Prep to exit the "Wait"
					{
						frameNo == "0" ? frameNo = "1" : frameNo = "0"; //change frame number
						textData = textData.substring(2);
						okAck = true;
						break;
					}
				}

				if (okAck) //Exit "Wait" part
				{
					break;
				}
			}
		}
	}
}

void ProtocolControl::receiver()
{
	String frame = this->rx->receiveStringFM(8);

	if (!frame.equals(""))
	{
		if (this->approveDataFrame(frame))
		{
			this->ackNo == "0" ? this->ackNo = "1" : this->ackNo = "0"; //Change Ack Number

			this->allReceiving += frame.substring(4, 6); //Store Incoming Data

			String ackFrame = this->makeAckFrame(ackNo, "0", destName);
			for (int i = 0; i < ackFrame.length(); i++)
			{
				this->tx->sendFM(ackFrame[i]);
			}

			if (frame[7] == '0') //End Of Transmission
			{
				this->ackNo = "0";
				//STORE DATA HERE
				this->allReceiving = "";
			}
		}
	}
}
