#ifndef __TSI721_FIFO_H__
#define __TSI721_FIFO_H__

static inline void SizeTsi721Fifo(const uint32_t entries, uint32_t& reg_size, uint32_t& mem_size)
{
  // This Be Gospel of Tsi721 Manual, Chapter 20, Page 737
  switch(entries) {
    case 32:       reg_size = 1;  break;
    case 64:       reg_size = 2;  break;
    case 128:      reg_size = 3;  break;
    case 256:      reg_size = 4;  break;
    case 512:      reg_size = 5;  break;
    case 1*1024:   reg_size = 6;  break;
    case 2*1024:   reg_size = 7;  break;
    case 4*1024:   reg_size = 8;  break;
    case 8*1024:   reg_size = 9;  break;
    case 16*1024:  reg_size = 10; break;
    case 32*1024:  reg_size = 11; break;
    case 64*1024:  reg_size = 12; break;
    case 128*1024: reg_size = 13; break;
    case 256*1024: reg_size = 14; break;
    case 512*1024: reg_size = 15; break;
    default: throw std::runtime_error("Non-standard FIFO size!"); break;
  }

  mem_size = entries * 64;
}

#endif // __TSI721_FIFO_H__
