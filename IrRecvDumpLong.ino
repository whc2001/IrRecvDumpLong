#define IR_IN_PIN 2                 // Infrared receiver pin (Physical pin, not interrupt ID)
#define BUFFER_SIZE 512             // Ring buffer size, will be cycled through so no need to set to too large value unless you are experiencing buffer overflow.
#define FINISH_THERSHOLD 65535UL    // Time to wait before timing out

const unsigned char intId = digitalPinToInterrupt(IR_IN_PIN);

// Ring buffer
unsigned int ringBuffer[BUFFER_SIZE];
unsigned int *readPtr = ringBuffer, *writePtr = ringBuffer;
unsigned char cycled = false, lose = false;

volatile unsigned long lastTimestamp = 0, nowTimestamp = 0, lastTimespan = 0;
unsigned long lastTimestampCopy = 0, nowTimestampCopy = 0, lastTimespanCopy = 0;
unsigned int bufferReadTmp;
unsigned char sendBuf[3];
unsigned char flag = true; // For the plus or minus sign before time value

void RingBufferWrite(unsigned int n)
{
    if(cycled && writePtr >= readPtr)
        lose = true;
    *writePtr++ = n;
    if(writePtr - ringBuffer >= BUFFER_SIZE)
    {
        cycled = true;
        writePtr = ringBuffer;
    }
}

bool RingBufferRead(unsigned int *out)
{
    if(readPtr == writePtr && !cycled)
        return false;
    unsigned int val = *readPtr++;
    if(readPtr - ringBuffer >= BUFFER_SIZE)
    {
        cycled = 0;
        readPtr = ringBuffer;
    }
    *out = val;
    return true;
}

void ExtIsr()
{
    nowTimestamp = micros();
    if(lastTimestamp)
        lastTimespan = nowTimestamp - lastTimestamp;
    lastTimestamp = nowTimestamp;
}

void AttachIsr()
{
    attachInterrupt(intId, ExtIsr, CHANGE);
}

void DetachIsr()
{
    detachInterrupt(intId);
}

void setup()
{
    Serial.begin(230400);
    pinMode(IR_IN_PIN, INPUT);
    AttachIsr();
}

void loop()
{
    // Detach ISR temporarily to prevent variable change during executing
    DetachIsr();
    lastTimestampCopy = lastTimestamp;
    nowTimestampCopy = micros();
    lastTimespanCopy = lastTimespan;
    lastTimespan = 0;
    AttachIsr();

    // Test for timeout (one packet is finished)
    if(lastTimestampCopy != 0 && (nowTimestampCopy > lastTimestampCopy) && (nowTimestampCopy - lastTimestampCopy) > FINISH_THERSHOLD)
    {
        lastTimestamp = 0;
        Serial.print(flag ? "+" : "-");
        Serial.println("0"); // Some interpreter (e.g. irdb.tk) require a dummy end sign to parse correctly
        flag = true;
    }

    // Save the last captured timespan
    if(lastTimespanCopy)
    {
        RingBufferWrite(lastTimespanCopy);
        lastTimespanCopy = 0;
    }

    // Read from ring buffer and send to serial port
    if(RingBufferRead(&bufferReadTmp))
    {
        Serial.print(flag ? "+" : "-");
        Serial.print(bufferReadTmp, DEC);
        Serial.print(" ");
        flag = !flag;
    }

    // Detect for ring buffer overflow
    if(lose)
    {
        Serial.println("!!! Ring buffer overrun, this packet is invalid !!!");
        lose = false;
    }
}
