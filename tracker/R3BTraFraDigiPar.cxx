#include <iostream>
#include "R3BTraFraDigiPar.h"
#include "TMath.h"

R3BTraFraDigiPar::R3BTraFraDigiPar(const char* name, const char* title, const char* context)
  : FairParGenericSet(name,title,context)
{
  clear();
}

void R3BTraFraDigiPar::putParams(FairParamList* list)
{
  std::cout<<"-I- R3BTraFraDigiPar::putParams() called"<<std::endl;
  if(!list) return;
//  list->add("max_paddle", (Int_t)nMaxPaddle);
//  list->add("max_plane", (Int_t)nMaxPlane);
}

Bool_t R3BTraFraDigiPar::getParams(FairParamList* list)
{
  std::cout<<"-I- R3BTraFraDigiPar::getParams() called"<<std::endl;
  if (!list) return kFALSE;
  std::cout<<"-I- R3BTraFraDigiPar::getParams() 1 ";
  
//  if (!list->fill("max_paddle", &nMaxPaddle)) return kFALSE;
//  if (!list->fill("max_plane", &nMaxPlane)) return kFALSE;
  return kTRUE;
}


void R3BTraFraDigiPar::Print()
{
    std::cout<<"-I- TraFra Digi Parameters:"<<std::endl;
//    std::cout<<"   Max Paddle   = "<<nMaxPaddle<<std::endl;
//    std::cout<<"   Max Plane   = "<<nMaxPlane<<std::endl;
}

ClassImp(R3BTraFraDigiPar);