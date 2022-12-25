#pragma once

class MyUpdaterVerifyClass; // forward declaration

class MyUpdaterHashClass : public UpdaterHashClass
{
public:
    virtual void begin()
    {
        ps = consume_data;
        crc = 0xffffffff;
        counter = 0;
        accum_le = 0;
        ref_crc = 0;
        ref_len = 0;
    }

    virtual void add(const void *data_, uint32_t data_len)
    {
        const uint8_t* data = (const uint8_t *)data_; // to allow us to index (legitimately)

        for (uint32_t i = 0; i < data_len; i++)
        {
            // Urgh, these fixed numbers are awful, but they are what we're given.
            // Evidently, we're relying on being handed contiguous data (ie offsetN+1 = offsetN + lenN)
            // but that's still true even if we maintained a running count of bytes for ourselves.
            switch (counter++) {
                case 4096 + 16 + 0: ps = consume_len;                            break;
                case 4096 + 16 + 4: ps = consume_crc;  ref_len = accum_le; break;
                case 4096 + 16 + 8: ps = consume_data; ref_crc = accum_le; break;
                default: break;
            }

            switch (ps) {
                case consume_data:
                    update_crc( crc, data[i] );
                    break;
                case consume_len:
                case consume_crc:
                    accum_le = ((uint32_t)data[i] << 24) | (accum_le >> 8);
                    update_crc( crc, 0 );
                    break;
            }
        }
    }

    virtual void end() { }
    virtual int len() { return sizeof(crc); }
    virtual const void *hash() { return (const void*)&crc; }
    virtual const unsigned char *oid() { return 0; }

private:
    enum ParseState { consume_data, consume_len, consume_crc };
    ParseState ps;
    uint32_t crc; // calculated
    uint32_t counter; // aka bytes seen
    uint32_t accum_le; // temporary little-endian accumulator
    uint32_t ref_crc; // provided
    uint32_t ref_len; // provided

    static void update_crc( uint32_t& crc, uint8_t x )
    {
        for (uint8_t mask = 0x80; mask > 0; mask >>= 1)
        {
            // !! (double not) converts any non-zero number to 1 and leaves zero alone
            crc = !!(crc & 0x80000000) ^ !!(x & mask)
                ? (crc << 1) ^ 0x04c11db7
                :  crc << 1;
        }
    }

    friend MyUpdaterVerifyClass;
};

// -------------------------------------

class MyUpdaterVerifyClass : public UpdaterVerifyClass
{
public:
    virtual bool verify(UpdaterHashClass *hash, const void *signature, uint32_t signatureLen)
    {
        (void)signature;
        (void)signatureLen;

        MyUpdaterHashClass* muh = static_cast< MyUpdaterHashClass* >(hash);
            // can't use dynamic_cast as apparently RTTI is disabled.
        if (muh)
        {
            //if (muh->crc != muh->ref_crc)
            //    Serial.printf("CRC is %08x vs expected %08x\n", muh->crc, muh->ref_crc);
            //if (muh->counter != muh->ref_len)
            //    Serial.printf("Length is %08x vs expected %08x\n", muh->counter, muh->ref_len);

            return (muh->crc == muh->ref_crc) and (muh->counter == muh->ref_len);
        }
        else
        {
            //Serial.printf("Cannot verify hash %p\n", hash);
            return false;
        }
    }

    virtual uint32_t length( ) { return 0; }
};
