#pragma once

#include "my_input_stream.h"
#include "lock_free_fifo.h"
//#include "conversion.h"

class my_msg_que_output_stream : public my_output_stream
{
public:
	static inline int32_t id_to_long(const char* id) // !! don't cope with les than 4 chars !!
	{
		assert((strlen(id) == 3 && id[3] == 0) || strlen(id) == 4);
		return (((int)id[0]) << 24) + (((int)id[1]) << 16) + (((int)id[2]) << 8) + id[3];
	}

    my_msg_que_output_stream(IWriteableQue* p_que, int p_handle = 0, const char* p_msg_id = 0) : m_que(p_que)
    {
    #if 0 //defined( _DEBUG )
	    p_que->DebugCheckThread(p_msg_id);
    #endif
//	    assert( ! p_que->isUncomitted() ); // did last user of que flush it?  Are more than 1 thread acessing que?
	    assert( p_handle != 0xcdcdcdcd );
	    //_RPT1(_CRT_WARN, "p_msg_id=%s\n", p_msg_id );

	    if( p_msg_id )
	    {
		    *this << p_handle;
		    *this << id_to_long(p_msg_id);
	    }
    }
    virtual void Write( const void* lpBuf, unsigned int nMax )
    {
	    m_que->pushString( nMax, (unsigned char*)lpBuf );
    }
    void Send()
    {
	    m_que->Send();
    }

private:
	IWriteableQue* m_que;
};
