/*****************************************************************************
 * Project: BaBar detector at the SLAC PEP-II B-factory
 * Package: RooFitCore
 *    File: $Id: RooFitResult.cc,v 1.7 2001/10/08 21:22:51 verkerke Exp $
 * Authors:
 *   WV, Wouter Verkerke, UC Santa Barbara, verkerke@slac.stanford.edu
 *   DK, David Kirkby, Stanford University, kirkby@hep.stanford.edu
 * History:
 *   17-Aug-2001 WV Created initial version
 *
 * Copyright (C) 2001 University of California
 *****************************************************************************/

// -- CLASS DESCRIPTION [MISC] --
// RooFitResult is a container class to hold the input and output
// of a PDF fit to a dataset. It contains:
//
//   - Values of all constant parameters
//   - Initial and final values of floating parameters with error
//   - Correlation matrix and global correlation coefficients
//   - NLL and EDM at mininum
//
// No references to the fitted PDF and dataset are stored

#include <iomanip.h>
#include "TMinuit.h"
#include "RooFitCore/RooFitResult.hh"
#include "RooFitCore/RooArgSet.hh"
#include "RooFitCore/RooArgList.hh"
#include "RooFitCore/RooRealVar.hh"

ClassImp(RooFitResult) 
;

RooFitResult::RooFitResult(const char* name, const char* title) : 
  TNamed(name,title), _constPars(0), _initPars(0), _finalPars(0), _globalCorr(0)
{  
  // Constructor
  appendToDir(this) ;
}

RooFitResult::~RooFitResult() 
{
  // Destructor
  if (_constPars) delete _constPars ;
  if (_initPars)  delete _initPars ;
  if (_finalPars) delete _finalPars ;
  if (_globalCorr) delete _globalCorr;

  _corrMatrix.Delete();

  removeFromDir(this) ;
}

void RooFitResult::setConstParList(const RooArgList& list) 
{
  // Fill the list of constant parameters
  if (_constPars) delete _constPars ;
  _constPars = (RooArgList*) list.snapshot() ;
}


void RooFitResult::setInitParList(const RooArgList& list)
{
  // Fill the list of initial values of the floating parameters 
  if (_initPars) delete _initPars ;
  _initPars = (RooArgList*) list.snapshot() ;
}


void RooFitResult::setFinalParList(const RooArgList& list)
{
  // Fill the list of final values of the floating parameters 
  if (_finalPars) delete _finalPars ;
  _finalPars = (RooArgList*) list.snapshot() ;
}


Double_t RooFitResult::correlation(const char* parname1, const char* parname2) const 
{
  // Return the correlation between parameters 'par1' and 'par2'

  const RooArgList* row = correlation(parname1) ;
  if (!row) return 0. ;
  RooAbsArg* arg = _initPars->find(parname2) ;
  if (!arg) {
    cout << "RooFitResult::correlation: variable " << parname2 << " not a floating parameter in fit" << endl ;
    return 0. ;
  }
  return ((RooRealVar*)row->at(_initPars->index(arg)))->getVal() ;
}


const RooArgList* RooFitResult::correlation(const char* parname) const 
{
  // Return the set of correlation coefficients of parameter 'par' with
  // all other floating parameters

  RooAbsArg* arg = _initPars->find(parname) ;
  if (!arg) {
    cout << "RooFitResult::correlation: variable " << parname << " not a floating parameter in fit" << endl ;
    return 0 ;
  }    
  return (RooArgList*)_corrMatrix.At(_initPars->index(arg)) ;
}


void RooFitResult::printToStream(ostream& os, PrintOption opt, TString indent) const
{
  // Print fit result to stream 'os'. In Verbose mode, the contant parameters and
  // the initial and final values of the floating parameters are printed. 
  // Standard mode only the final values of the floating parameters are printed

  os << endl 
     << "  RooFitResult: minimized NLL value: " << _minNLL << ", estimated distance to minimum: " << _edm 
     << endl 
     << endl ;

  Int_t i ;
  if (opt>=Verbose) {
    if (_constPars->getSize()>0) {
      os << "    Constant Parameter    Value     " << endl
	 << "  --------------------  ------------" << endl ;

      for (i=0 ; i<_constPars->getSize() ; i++) {
	os << "  " << setw(20) << ((RooAbsArg*)_constPars->at(i))->GetName()
	   << "  " << setw(12) << Form("%12.4e",((RooRealVar*)_constPars->at(i))->getVal())
	   << endl ;
      }

      os << endl ;
    }


    os << "    Floating Parameter  InitialValue    FinalValue +/-  Error     GblCorr." << endl
       << "  --------------------  ------------  --------------------------  --------" << endl ;

    for (i=0 ; i<_finalPars->getSize() ; i++) {
      os << "  "    << setw(20) << ((RooAbsArg*)_finalPars->at(i))->GetName() ;
      os << "  "    << setw(12) << Form("%12.4e",((RooRealVar*)_initPars->at(i))->getVal())
	 << "  "    << setw(12) << Form("%12.4e",((RooRealVar*)_finalPars->at(i))->getVal())
	 << " +/- " << setw(9)  << Form("%9.2e",((RooRealVar*)_finalPars->at(i))->getError())
	 << "  "    << setw(8)  << Form("%8.6f" ,((RooRealVar*)_globalCorr->at(i))->getVal())
	 << endl ;
    }

  } else {
    os << "    Floating Parameter    FinalValue +/-  Error   " << endl
       << "  --------------------  --------------------------" << endl ;

    for (i=0 ; i<_finalPars->getSize() ; i++) {
      os << "  "    << setw(20) << ((RooAbsArg*)_finalPars->at(i))->GetName()
	 << "  "    << setw(12) << Form("%12.4e",((RooRealVar*)_finalPars->at(i))->getVal())
	 << " +/- " << setw(9)  << Form("%9.2e",((RooRealVar*)_finalPars->at(i))->getError())
	 << endl ;
    }
  }
  

  os << endl ;
}


void RooFitResult::fillCorrMatrix()
{
  // Extract the correlation matrix and the global correlation coefficients from the MINUIT memory buffer 
  // and fill the internal arrays.

  // Sanity check
  if (gMinuit->fNpar <= 1) {
    cout << "RooFitResult::fillCorrMatrix: number of floating parameters <=1, correlation matrix not filled" << endl ;
    return ;
  }

  if (!_initPars) {
    cout << "RooFitResult::fillCorrMatrix: ERROR: list of initial parameters must be filled first" << endl ;
    return ;
  }

  // Delete eventual prevous correlation data holders
  if (_globalCorr) delete _globalCorr ;

  _corrMatrix.Delete();

  // Build holding arrays for correlation coefficients
  _globalCorr = new RooArgList("globalCorrelations") ;
  TIterator* vIter = _initPars->createIterator() ;
  RooAbsArg* arg ;
  Int_t idx(0) ;
  while(arg=(RooAbsArg*)vIter->Next()) {
    // Create global correlation value holder
    TString gcName("GC[") ;
    gcName.Append(arg->GetName()) ;
    gcName.Append("]") ;
    TString gcTitle(arg->GetTitle()) ;
    gcTitle.Append(" Global Correlation") ;
    _globalCorr->addOwned(*(new RooRealVar(gcName.Data(),gcTitle.Data(),0.))) ;

    // Create array with correlation holders for this parameter
    TString name("C[") ;
    name.Append(arg->GetName()) ;
    name.Append(",*]") ;
    RooArgList* corrMatrixRow = new RooArgList(name.Data()) ;
    _corrMatrix.Add(corrMatrixRow) ;
    TIterator* vIter2 = _initPars->createIterator() ;
    RooAbsArg* arg2 ;
    while(arg2=(RooAbsArg*)vIter2->Next()) {

      TString cName("C[") ;
      cName.Append(arg->GetName()) ;
      cName.Append(",") ;
      cName.Append(arg2->GetName()) ;
      cName.Append("]") ;
      TString cTitle("Correlation between ") ;
      cTitle.Append(arg->GetName()) ;
      cTitle.Append(" and ") ;
      cTitle.Append(arg2->GetName()) ;
      corrMatrixRow->addOwned(*(new RooRealVar(cName.Data(),cTitle.Data(),0.))) ;      
    }
    delete vIter2 ;
    idx++ ;
  }
  delete vIter ;

  TIterator *gcIter = _globalCorr->createIterator() ;

  // Extract correlation information for MINUIT (code taken from TMinuit::mnmatu() )

  // WVE: This code directly manipulates minuit internal workspace, 
  //      if TMinuit code changes this may need updating
  Int_t ndex, i, j, m, n, ncoef, nparm, id, it, ix;
  Int_t ndi, ndj, iso, isw2, isw5;
  ncoef = (gMinuit->fNpagwd - 19) / 6;
  nparm = TMath::Min(gMinuit->fNpar,ncoef);
  RooRealVar* gcVal(0) ;
  for (i = 1; i <= gMinuit->fNpar; ++i) {
    ix  = gMinuit->fNexofi[i-1];
    ndi = i*(i + 1) / 2;
    for (j = 1; j <= gMinuit->fNpar; ++j) {
      m    = TMath::Max(i,j);
      n    = TMath::Min(i,j);
      ndex = m*(m-1) / 2 + n;
      ndj  = j*(j + 1) / 2;
      gMinuit->fMATUvline[j-1] = gMinuit->fVhmat[ndex-1] / TMath::Sqrt(TMath::Abs(gMinuit->fVhmat[ndi-1]*gMinuit->fVhmat[ndj-1]));
    }
    nparm = TMath::Min(gMinuit->fNpar,ncoef);

    gcVal = (RooRealVar*) gcIter->Next() ;
    gcVal->setVal(gMinuit->fGlobcc[i-1]) ;
    TIterator* cIter = ((RooArgList*)_corrMatrix.At(i-1))->createIterator() ;
    for (it = 1; it <= gMinuit->fNpar ; ++it) {
      RooRealVar* cVal = (RooRealVar*) cIter->Next() ;
      cVal->setVal(gMinuit->fMATUvline[it-1]) ;
    }
    delete cIter ;
  }


  delete gcIter ;
} 


