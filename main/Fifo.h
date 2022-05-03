#ifndef FIFO_H
#define FIFO_H



class Fifo
{
    public:
        Fifo(int size);
        void push(short value);
        short pop();
        int size();
        ~Fifo();

};

#endif //Fifo_h