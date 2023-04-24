#include "Serializer.h"


void Serializer::Init(uint16_t id)
{
    writePosition = 0;
    flashPosition = 0;
    ffs_open(GetFilesystem(), &writeFile, id);
    memset(data, 0, 256);
}
uint8_t Serializer::GetNextValue()
{
    uint8_t res;
    ffs_read(GetFilesystem(), &writeFile, &res, 1);
    return res;
}
void Serializer::AddData(uint8_t val)
{
    data[writePosition++] = val;
    if(writePosition>=256)
    {
        writePosition = 0;
        FlushToFlash();
    }
}

void Serializer::Finish() 
{
    FlushToFlash();
}

void Serializer::Erase()
{ 
    ffs_erase(GetFilesystem(), &writeFile);
}

void Serializer::FlushToFlash()
{
    ffs_append(GetFilesystem(), &writeFile, data, 256);
    memset(data, 0, 256);
}