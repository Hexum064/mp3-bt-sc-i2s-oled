#include "Fifo.h"
#include <stdlib.h> 

short* buffer;
int max_size = 0;
int buffer_head = 0;
int buffer_size = 0;
int buffer_tail = 0;

Fifo::Fifo(int size) 
{
    max_size = size;
    buffer = (short *)calloc(size, sizeof(short));
}

Fifo::~Fifo()
{
    free(buffer);
}

void Fifo::push(short value)
{

    if (buffer_size < max_size - 1)
    {
        buffer_size++;

        buffer[buffer_head] = value;
        
        buffer_head++;

        if (buffer_head == max_size)
        {
            buffer_head = 0;
        }

        return;
    }

}

short Fifo::pop()
{

    short value = 0;

    if (buffer_size > 0)
    {
        buffer_size--;

        value = buffer[buffer_tail];
        
        buffer_tail++;

        if (buffer_tail == max_size)
        {
            buffer_tail = 0;
        }

               
    }

    return value; 
}

int Fifo::size()
{
    return buffer_size;
}

