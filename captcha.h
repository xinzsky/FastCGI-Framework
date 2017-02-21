#ifndef CAPTCHA_H
#define CAPTCHA_H

extern const int gifsize;

#ifdef __cplusplus
extern "C"
{
#endif


  void captcha(unsigned char im[70*200], unsigned char l[6]);
  void makegif(unsigned char im[70*200], unsigned char gif[gifsize]);


#ifdef __cplusplus
}
#endif

#endif


