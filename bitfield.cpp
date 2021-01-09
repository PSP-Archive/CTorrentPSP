#include "config.h"
#include "bitfield.h"

#ifdef WINDOWS
#include <io.h>
#include <memory.h>
#else
#include <unistd.h>
#include <sys/param.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef HAVE_RANDOM
#include "compat.h"
#endif

const unsigned char BIT_HEX[] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

#define _isset(idx)		(b[(idx) / 8 ] & BIT_HEX[(idx) % 8])
#define _isempty() 		(nset == 0)
#define _isempty_sp(sp) 	((sp).nset == 0)
#define _isfull() 		(nset >= nbits)
#define _isfull_sp(sp) 		((sp).nset >= nbits)

#define THROWERR { printf("throw 9 - %s %d\n", __FILE__, __LINE__); }

size_t BitField::nbytes = 0;
size_t BitField::nbits = 0;

BitField::BitField()
{
  b = new unsigned char[nbytes];
#ifndef WINDOWS  
  if( !b ) THROWERR;
#endif

  memset(b, 0, nbytes);
  nset = 0;
}

BitField::BitField(size_t npcs)	
{
  nbits = npcs;
  nbytes = nbits / 8;
  if( nbits % 8 ) nbytes++;

  b = new unsigned char[nbytes];
#ifndef WINDOWS
  if( !b ) THROWERR;
#endif

  memset(b, 0, nbytes);
  nset = 0;
}

BitField::BitField(const BitField &bf)
{
  nset = bf.nset;
  if( _isfull_sp(bf) ) b = (unsigned char *) 0;
  else{
    b = new unsigned char[nbytes];
#ifndef WINDOWS
    if( !b ) THROWERR;
#endif
    memcpy(b, bf.b, nbytes);
  }
}

void BitField::operator=(const BitField &bf)
{
  nset = bf.nset;
  if( _isfull_sp(bf) ){
    if( b ) { delete []b; b = (unsigned char*) 0; }
  }else{
    if( !b ){ 
      b = new unsigned char[nbytes];
#ifndef WINDOWS
      if( !b ) THROWERR;
#endif
    }
    memcpy(b, bf.b, nbytes);
  }
}

// _set() sets the bit but doesn't increment nset or set the isfull case.
// Use instead of Set() when you know nset is incorrect and will be corrected
// afterward (as in Invert or by _recalc),
// and either bitfield won't get full or you'll _recalc() afterward to fix it.
inline void BitField::_set(size_t idx)
{
  if( idx < nbits && !_isfull() && !_isset(idx) )
    b[idx / 8] |= BIT_HEX[idx % 8];
}

inline void BitField::_setall(unsigned char *buf)
{
  size_t i;

  memset(buf,0xFF,nbytes - 1);

  if( nbits % 8 ){
    buf[nbytes - 1] = ~(BIT_HEX[nbits % 8 - 1] - 1);
  }else
    buf[nbytes - 1] = (unsigned char) 0xFF;
}

inline void BitField::_recalc()
{
  // ���¼��� nset ��ֵ
  static unsigned char BITS[256] = {0xff};
  size_t i;

  if( BITS[0] ){  // initialize bitcounts
    size_t j, exp, x;
    BITS[0] = 0;
    x = 0;
    for(i=0; i<8; i++){
      exp = 1<<i;
      for(j=0; j < exp; j++)
        BITS[++x] = BITS[j] + 1;
    }
  }

  for(nset = 0, i = 0; i < nbytes; i++)
    nset += BITS[b[i]];
  if( _isfull() && b ){ delete []b; b = (unsigned char*) 0;}
}

void BitField::SetAll()
{
  if( b ){
    delete []b; 
    b = (unsigned char*) 0;
  }
  nset = nbits;
}

void BitField::Clear()
{
  if( _isfull() ){
    b = new unsigned char[nbytes];
#ifndef WINDOWS
    if( !b ) THROWERR;
#endif
  }
  memset(b, 0, nbytes);
  nset = 0;
}

int BitField::IsSet(size_t idx) const
{
  if( idx >= nbits ) return 0;
  return _isfull() ? 1 : _isset(idx);
}

void BitField::Set(size_t idx)
{
  if(idx >= nbits) return;

  if( !_isfull() && !_isset(idx) ){
    b[idx / 8] |= BIT_HEX[idx % 8];
    nset++;
    if( _isfull() && b){ delete []b; b = (unsigned char*) 0;}
  }
}

void BitField::UnSet(size_t idx)
{
  if( idx >= nbits ) return;

  if( _isfull() ){
    b = new unsigned char[nbytes];
#ifndef WINDOWS
    if( !b ) THROWERR;
#endif
    _setall(b);
    b[idx / 8] &= (~BIT_HEX[idx % 8]);
    nset = nbits - 1;
  }else{
    if( _isset(idx) ){
      b[idx / 8] &= (~BIT_HEX[idx % 8]);
      nset--;
    }
  }
}

void BitField::Invert()
{
  if( _isempty() ){
    SetAll();
  }else if( _isfull() ){
    Clear();
  }else{
    size_t i = 0;
    size_t s = nset;
    for( ; i < nbytes - 1; i++ ) b[i] = ~b[i];

    if( nbits % 8 ){
      for( i = 8 * (nbytes - 1); i < nbits; i++ ){
        if( _isset(i) ) UnSet(i);
        else _set(i);
      }
    }else b[nbytes - 1] = ~b[nbytes - 1];

    nset = nbits - s;
  }
}

// Combine (Logical "OR")
void BitField::Comb(const BitField &bf)
{
  size_t i;
  if( !_isempty_sp(bf) && !_isfull() ){
    if( _isfull_sp(bf) ){
      SetAll();
    }else if( _isempty() ){
      memcpy(b, bf.b, nbytes);
      nset = bf.nset;
    }else{
      for(i = 0; i < nbytes; i++) b[i] |= bf.b[i];
      _recalc();
    }
  }
}

void BitField::Except(const BitField &bf)
{
  size_t i;

  if( !_isempty_sp(bf) && !_isempty() ){
    if( _isfull_sp(bf) ){
      Clear();
    }else{
      if( _isfull() ){
        b = new unsigned char[nbytes];
#ifndef WINDOWS
        if( !b ) THROWERR;
#endif
        _setall(b);
      }
      for(i = 0; i < nbytes; i++) b[i] &= ~bf.b[i];
      _recalc();
    }
  }
}

void BitField::And(const BitField &bf)
{
  size_t i;

  if( !_isfull_sp(bf) && !_isempty() ){
    if( _isempty_sp(bf) ){
      Clear();
    }else{
      if( _isfull() ){
        b = new unsigned char[nbytes];
#ifndef WINDOWS
        if( !b ) THROWERR;
#endif
        memcpy(b, bf.b, nbytes);
        nset = bf.nset;
      }else{
        for(i = 0; i < nbytes; i++) b[i] &= bf.b[i];
        _recalc();
      }
    }
  }
}

size_t BitField::Random() const
{
  size_t idx;

  if( _isfull() ) idx = rand() % nbits;
  else{
    size_t i;
    i = rand() % nset + 1;
    for(idx = 0; idx < nbits && i; idx++) 
      if( _isset(idx) ) i--;
    idx--;
  }
  return idx;
}

void BitField::SetReferBuffer(char *buf)
{
  if( !b ){ 
    b = new unsigned char[nbytes];
#ifndef WINDOWS
    if( !b ) THROWERR;
#endif
  }
  memcpy((char*)b,buf,nbytes);
  if( nbits % 8 )
    b[nbytes - 1] &= ~(BIT_HEX[nbits % 8 - 1] - 1);
  _recalc();
}

void BitField::WriteToBuffer(char *buf)
{
  if(_isfull())
    _setall((unsigned char*)buf);
  else
    memcpy(buf,(char*)b,nbytes);
}

int BitField::SetReferFile(const char *fname)
{
  FILE *fp;
  struct stat sb;
  char *bitbuf = (char*) 0;

  if(stat(fname, &sb) < 0) return -1;
  if( (size_t)sb.st_size != nbytes ) return -1;
  
  fp = fopen(fname, "r");
  if( !fp ) return -1;
  
  bitbuf = new char[nbytes];
#ifndef WINDOWS
  if( !bitbuf ) goto fclose_err;
#endif

  if( fread(bitbuf, nbytes, 1, fp) != 1 ) goto fclose_err;

  fclose(fp);
  
  SetReferBuffer(bitbuf);

  delete []bitbuf;
  return 0;
 fclose_err:
  if( bitbuf ) delete []bitbuf;
  fclose(fp);
  return -1;
}

int BitField::WriteToFile(const char *fname)
{
  FILE *fp;
  char *bitbuf = (char*) 0;

  fp = fopen(fname, "w");
  if( !fp ) return -1;
  
  bitbuf = new char[nbytes];
#ifndef WINDOWS
  if( !bitbuf ) goto fclose_err;
#endif

  WriteToBuffer(bitbuf);

  if( fwrite(bitbuf, nbytes, 1, fp) != 1 ) goto fclose_err;

  delete []bitbuf;
  fclose(fp);
  return 0;
 fclose_err:
  if( bitbuf ) delete []bitbuf;
  fclose(fp);
  return -1;
}

