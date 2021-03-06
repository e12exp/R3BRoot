/******************************************************************************
 *   Copyright (C) 2019 GSI Helmholtzzentrum für Schwerionenforschung GmbH    *
 *   Copyright (C) 2019 Members of R3B Collaboration                          *
 *                                                                            *
 *             This software is distributed under the terms of the            *
 *                 GNU General Public Licence (GPL) version 3,                *
 *                    copied verbatim in the file "LICENSE".                  *
 *                                                                            *
 * In applying this license GSI does not waive the privileges and immunities  *
 * granted to it by virtue of its status as an Intergovernmental Organization *
 * or submit itself to any jurisdiction.                                      *
 ******************************************************************************/

// ------------------------------------------------------------------------------
// -----                  R3BRoluCal2Hit                                   ------
// -----            Created July 2019 by A. Kelic-Heil                     ------
// -----            Modified July 2021 by A. Kelic-Heil to include 2 ROLUs ------
// ------------------------------------------------------------------------------

#include "R3BRoluCal2Hit.h"
#include "FairLogger.h"
#include "R3BRoluCalData.h"
#include "R3BRoluHitData.h"
#include "R3BRoluMapped2Cal.h"
#include "R3BRoluMappedData.h"
#include "R3BTCalEngine.h"
#include "R3BTCalPar.h"

#include "TClonesArray.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TMath.h"
#include <fstream>
#include <iomanip>
#include <iostream>

#include <boost/multi_array.hpp>
#include <cassert>
#include <cmath> // NAN

template <class T>
static void init_array(T& array, double init)
{
    // boost::multi_array prefers cascaded for loops over index sets
    // (e.g. python's dict().keys())
    // thus we can not really use a range based for loop
    // and instead wrangle pointers like it is 1980 again.
    for (double* e = array.origin(); e < array.origin() + array.num_elements(); ++e)
        *e = init;
}
using namespace std;
#define IS_NAN(x) TMath::IsNaN(x)

R3BRoluCal2Hit::R3BRoluCal2Hit()
    : FairTask("RoluCal2Hit", 1)
    , fCalItems(NULL)
    , fHitItems(new TClonesArray("R3BRoluHitData"))
    , fNofHitItems(0)
    , fNofDetectors(2)
    , fClockFreq(1. / VFTX_CLOCK_MHZ * 1000.)
{
    for (Int_t iDet = 0; iDet < fNofDetectors; iDet++)
    {
        fhQ_R[iDet] = NULL;
        fhQ_O[iDet] = NULL;
        fhQ_L[iDet] = NULL;
        fhQ_U[iDet] = NULL;
    }
}

R3BRoluCal2Hit::R3BRoluCal2Hit(const char* name, Int_t iVerbose)
    : FairTask(name, iVerbose)
    , fCalItems(NULL)
    , fHitItems(new TClonesArray("R3BRoluHitData"))
    , fNofHitItems(0)
    , fNofDetectors(2)
    , fClockFreq(1. / VFTX_CLOCK_MHZ * 1000.)
{
    for (Int_t iDet = 0; iDet < fNofDetectors; iDet++)
    {
        fhQ_R[iDet] = NULL;
        fhQ_O[iDet] = NULL;
        fhQ_L[iDet] = NULL;
        fhQ_U[iDet] = NULL;
    }
}

R3BRoluCal2Hit::~R3BRoluCal2Hit()
{
    for (Int_t iDet = 0; iDet < fNofDetectors; iDet++)
    {
        if (fhQ_L[iDet])
            delete (fhQ_L[iDet]);
        if (fhQ_R[iDet])
            delete (fhQ_R[iDet]);
        if (fhQ_O[iDet])
            delete (fhQ_O[iDet]);
        if (fhQ_U[iDet])
            delete (fhQ_U[iDet]);
    }

    if (fHitItems)
    {
        delete fHitItems;
        fHitItems = NULL;
    }
}

InitStatus R3BRoluCal2Hit::Init()
{
    // get access to Cal data
    FairRootManager* mgr = FairRootManager::Instance();
    if (NULL == mgr)
        LOG(fatal) << "FairRootManager not found";

    fCalItems = (TClonesArray*)mgr->GetObject("RoluCal");
    if (NULL == fCalItems)
        LOG(fatal) << "Branch RoluCal not found";

    // request storage of Hit data in output tree
    mgr->Register("RoluHit", "Land", fHitItems, kTRUE);

    Icount = 0;

    return kSUCCESS;
}

InitStatus R3BRoluCal2Hit::ReInit() { return kSUCCESS; }

/*
 * Calculate a single hit time for Rolu detector
 */
void R3BRoluCal2Hit::Exec(Option_t* option)
{
    Icount++;

    // min,max,Nbins for ToT spectra
    Double_t fhQmin = 0.;
    Double_t fhQmax = 200; // 300.; //150
    Int_t fhQbin = 4000;   // 0; //3000; //1500
    for (Int_t iDet = 0; iDet < fNofDetectors; iDet++)
    {
        if (NULL == fhQ_R[iDet])
        {
            fhQ_R[iDet] = new TH1F(Form("TOT_R%d", iDet + 1), Form("TOTr%d", iDet + 1), fhQbin, fhQmin, fhQmax);
        }

        if (NULL == fhQ_O[iDet])
        {
            fhQ_O[iDet] = new TH1F(Form("TOT_O%d", iDet + 1), Form("TOTo%d", iDet + 1), fhQbin, fhQmin, fhQmax);
        }

        if (NULL == fhQ_L[iDet])
        {
            fhQ_L[iDet] = new TH1F(Form("TOT_L%d", iDet + 1), Form("TOTl%d", iDet + 1), fhQbin, fhQmin, fhQmax);
        }

        if (NULL == fhQ_U[iDet])
        {
            fhQ_U[iDet] = new TH1F(Form("TOT_U%d", iDet + 1), Form("TOTu%d", iDet + 1), fhQbin, fhQmin, fhQmax);
        }
    }

    Int_t nParts = fCalItems->GetEntriesFast();

    if (nParts < 1)
        return;

    Int_t iDet = 0;
    /*
     * Note: double x[nParts][2][4]={NAN};
     * will initialize everything other than x[0][0][0] to 0.0.
     * (and also not compile with our ancient gcc 4.8.5.)
     */
    using A = boost::multi_array<double, 3>;
    auto dims = boost::extents[nParts][fNofDetectors][4];
    A timeRolu_L(dims);
    init_array(timeRolu_L, NAN);
    A timeRolu_T(dims);
    init_array(timeRolu_T, NAN);
    A totRolu(dims);
    init_array(totRolu, NAN);

    for (Int_t iPart = 0; iPart < nParts; iPart++)
    {
        /*
         * nParts is the number of particle passing through detector in one event
         */
        R3BRoluCalData* calItem = (R3BRoluCalData*)fCalItems->At(iPart);
        iDet = calItem->GetDetector();

        for (Int_t iCha = 0; iCha < 4; iCha++)
        {

            if (!(IS_NAN(calItem->GetTimeL_ns(iCha))))
            { // TAMEX leading
                timeRolu_L[iPart][iDet - 1][iCha] = calItem->GetTimeL_ns(iCha);
            }
            if (!(IS_NAN(calItem->GetTimeT_ns(iCha))))
            { // TAMEX trailing
                timeRolu_T[iPart][iDet - 1][iCha] = calItem->GetTimeT_ns(iCha);
            }

            if (timeRolu_T[iPart][iDet - 1][iCha] > 0. && timeRolu_L[iPart][iDet - 1][iCha] > 0. &&
                !(IS_NAN(timeRolu_T[iPart][iDet - 1][iCha])) && !(IS_NAN(timeRolu_L[iPart][iDet - 1][iCha])))
            {
                while (timeRolu_T[iPart][iDet - 1][iCha] - timeRolu_L[iPart][iDet - 1][iCha] <= 0.)
                {
                    timeRolu_T[iPart][iDet - 1][iCha] = timeRolu_T[iPart][iDet - 1][iCha] + 2048. * fClockFreq;
                }

                totRolu[iPart][iDet - 1][iCha] = timeRolu_T[iPart][iDet - 1][iCha] - timeRolu_L[iPart][iDet - 1][iCha];
            }

            // cout<<"ROLU cal2hit: "<<iDet<<", "<<iCha+1<<"; "<<timeRolu_L[iPart][iDet - 1][iCha]<<",
            // "<<totRolu[iPart][iDet - 1][iCha]<<endl;
            new ((*fHitItems)[fNofHitItems])
                R3BRoluHitData(iDet, iCha + 1, timeRolu_L[iPart][iDet - 1][iCha], totRolu[iPart][iDet - 1][iCha]);
            fNofHitItems += 1;
        }

        fhQ_R[iDet - 1]->Fill(totRolu[iPart][iDet - 1][0]);
        fhQ_O[iDet - 1]->Fill(totRolu[iPart][iDet - 1][1]);
        fhQ_L[iDet - 1]->Fill(totRolu[iPart][iDet - 1][2]);
        fhQ_U[iDet - 1]->Fill(totRolu[iPart][iDet - 1][3]);
    }
}

void R3BRoluCal2Hit::FinishEvent()
{

    if (fHitItems)
    {
        fHitItems->Clear();
        fNofHitItems = 0;
    }
}

void R3BRoluCal2Hit::FinishTask()
{
    for (Int_t iDet = 0; iDet < fNofDetectors; iDet++)
    {
        if (fhQ_L[iDet])
            fhQ_L[iDet]->Write();
        if (fhQ_R[iDet])
            fhQ_R[iDet]->Write();
        if (fhQ_O[iDet])
            fhQ_O[iDet]->Write();
        if (fhQ_U[iDet])
            fhQ_U[iDet]->Write();
    }
}

ClassImp(R3BRoluCal2Hit)
