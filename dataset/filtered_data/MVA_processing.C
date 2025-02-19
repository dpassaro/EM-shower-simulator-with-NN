#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <math.h>
#include <TFile.h>
#include <TChain.h>
#include <TTree.h>
#include <TBranch.h>
#include <TCanvas.h>
#include <TF1.h>
#include <TH2.h>

// Please change this values accordingly to the geometry of the detector:

int const NUMBER_OF_LAYERS = 12;
int const NUMBER_OF_PIXEL_Z = 25;
int const NUMBER_OF_PIXEL_Y = 25;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

array<double,2> compute_angles(double x_start, double y_start, double z_start,
                               double x_stop, double y_stop, double z_stop){
  /**
  Compute the angles theta and phi of the primary particle trajectory
  */
  double x = x_stop - x_start;
  double y = y_stop - y_start;
  double z = z_stop - z_start;
  array<double,2> angles;
  angles[0] = TMath::ATan((x*x + y*y)/z);// theta
  angles[1] = TMath::ATan(y/x);
  return angles;
}

void null(double shower[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1]){
  /**
  Function to put to zero each value of a 3D x 1 vector
  */
  for(int layers=0; layers<NUMBER_OF_LAYERS; layers++){
    for(int num_z=0; num_z<NUMBER_OF_PIXEL_Z;num_z++){
      for(int num_y=0; num_y<NUMBER_OF_PIXEL_Y;num_y++){
        shower[layers][num_z][num_y][0] = 0 ;
      }
    }
  }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

void MVA_processing(){
  /**
  Function to shape shower.root contents, generated by gears.exe, to the shape
  desidered to perform a multivariate analysis using TensorFlow in Python.
  */
    ROOT::EnableImplicitMT(); // Tell ROOT you want to go parallel

    // input path = path where shower.root (generated with Geant) is located
    const char *input = "/mnt/d/Users/Daniele/EM_shower/shower_1.root";
    const char *output = "data_MVA_24pixel.root";

  	TChain *t = new TChain("t"); // create a chain of Ttree "t"
  	t->Add(input);
  	int nStepPoints; // number of Geant4 step points
  	t->SetBranchAddress("n",&nStepPoints);
  	// parameters of step points
    vector<double> *x_=0, *y_=0, *z_=0, *de_=0, *k_=0, *et_=0;
  	vector<int> *pid_=0, *pdg_=0, *vlm_=0; // copy number of a Geant4 volume
  	TBranch *bx, *by, *bz, *be, *bv, *bk, *bp, *bet, *bpid;
  	t->SetBranchAddress("x",&x_, &bx); // global x
  	t->SetBranchAddress("y",&y_, &by); // global y
  	t->SetBranchAddress("z",&z_, &bz); // global z
  	t->SetBranchAddress("de",&de_, &be); // energy deposition
    t->SetBranchAddress("k",&k_, &bk); // kinetic energy
    t->SetBranchAddress("et",&et_, &bet); // total energy deposition
  	t->SetBranchAddress("vlm",&vlm_, &bv); // sensitive volume
  	t->SetBranchAddress("pdg",&pdg_, &bp); //particle
    t->SetBranchAddress("pid",&pid_, &bpid); //particle parent

   	// output
  	TFile *file = new TFile(output, "recreate");
  	TTree *tree = new TTree("h","ttree");
  	int evt; // id of event from Geant4 simulation
    int primary;
    double theta, phi, en_in, en_mis;
    tree->Branch("evt",&evt,"evt/I");
    tree->Branch("primary",&primary, "primary/I");
    tree->Branch("en_in", &en_in, "en_in/D");
    tree->Branch("en_mis", &en_mis, "en_mis/D");
    tree->Branch("theta", &theta, "theta/D");
    tree->Branch("phi", &phi, "phi/D");
    double shower[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1];
    // 3D x 1 vector: first index = layer; second index = z-coordinate of unit cell;
    // third index = y-coordinate of unit cell
    char shower_name[50];  //sprintf for dynamical string sum and append
    sprintf(shower_name, "shower[%d][%d][%d][1]/D", NUMBER_OF_LAYERS,
            NUMBER_OF_PIXEL_Z,NUMBER_OF_PIXEL_Y);
    tree->Branch("shower", &shower, shower_name);

    double x_start, y_start, z_start;
    double x_stop, y_stop, z_stop;
    array<double,2> angles;
    Bool_t check_en, check_start, check_stop;
    double check_en_vol=0;

  	// main loop
  	int nevt = t->GetEntries(); // total number of events simulated
  	cout<<nevt<<" events to be processed"<<endl;
    double milliseconds;
    std::chrono::steady_clock::time_point begin, end;
    begin = std::chrono::steady_clock::now();
    int step_point;
    for (evt=0; evt<nevt; evt++) {
      if (evt%50==0){
        cout<<evt<<" events processed "<<endl;
      }
      t->GetEntry(evt); // get information from input tree
      check_en = kFALSE; check_start = kFALSE; check_stop = kFALSE;
      null(shower);
      step_point=0;
      //////////////////////////////////////////////////////////////////////////
      // Find the angles
      while(!check_en || !check_start || !check_stop) {
        if(vlm_->at(step_point) == 1 && k_->at(step_point)>100000){
          en_in = k_->at(step_point);
          if(pdg_->at(step_point)==22) primary = 0;
          if(pdg_->at(step_point)==11) primary = 1;
          if(pdg_->at(step_point)==-11) primary = -1;
          check_en=kTRUE;
        }
        if(pid_->at(step_point)==0 && x_->at(step_point)<-180){
          x_start = x_->at(step_point); y_start = y_->at(step_point); z_start = z_->at(step_point);
          check_start = kTRUE;
        }
        if( (pid_->at(step_point)==0 || pid_->at(step_point)==1)&& x_->at(step_point)>-30 ){//|| pid_->at(step_point)==1)
          x_stop = x_->at(step_point); y_stop = y_->at(step_point); z_stop = z_->at(step_point);
          check_stop = kTRUE;
          angles = compute_angles(x_start,y_start,z_start,x_stop,y_stop,z_stop);
          theta = angles[0]; phi = angles[1];
        }
        step_point++;
      }
      //////////////////////////////////////////////////////////////////////////
      //for loops have to be from 1 to NUMBER_OF_*+1, do not change
      for(int layers=1; layers<NUMBER_OF_LAYERS+1; layers++){
        int j=0;
        for(int num_z=1; num_z<NUMBER_OF_PIXEL_Z+1;num_z++){
          for(int num_y=1; num_y<NUMBER_OF_PIXEL_Y+1;num_y++){
            j++;
            if( layers*1000+j>=et_[0].size() || et_[0][layers*1000+j]==0){
              shower[layers-1][num_z-1][num_y-1][0]=-5;
            }
            else{
              shower[layers-1][num_z-1][num_y-1][0]=TMath::Log10(et_[0][layers*1000+j]);
            }
          }
        }
      }
      //////////////////////////////////////////////////////////////////////////
      if(evt%50==0){
        end = std::chrono::steady_clock::now();
        cout <<"time per event:\t"<<
        std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count()/1000./(evt+1) <<" s"<<endl;
      }
      en_mis = et_[0][0];
      tree->Fill();
    }
    end = std::chrono::steady_clock::now();
    milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    std::cout << "Total elapsed time = " << milliseconds/1000. << "s" << std::endl;

  	// save the output tree
    tree->Write("", TObject::kWriteDelete); // write tree, then delete previous
    file->Close(); // close output file
}

double MVA_processing_normalization(){
  /**
  Measures the max deposited energy in each cell as normalization constant.
  */
  ROOT::EnableImplicitMT(); // Tell ROOT you want to go parallel

  const char *input="data_MVA_24pixel.root";
  TChain *h = new TChain("h");
  h->Add(input);

  double shower[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1];
  TBranch *b_shower, *b_en_in, *b_pid, *b_en_mis;
  double en_in, en_mis;
  int pid;
  h->SetBranchAddress("primary", &pid, &b_pid);
  h->SetBranchAddress("en_in", &en_in, &b_en_in);
  h->SetBranchAddress("en_mis", &en_mis, &b_en_mis);
  h->SetBranchAddress("shower", shower, &b_shower);

  double max = 0;
  for(int evt=0; evt<h->GetEntries(); evt++){
    h->GetEntry(evt);
    for(int layer=0; layer<NUMBER_OF_LAYERS; layer++){
      for(int num_z=0; num_z<NUMBER_OF_PIXEL_Z;num_z++){
        for(int num_y=0; num_y<NUMBER_OF_PIXEL_Y;num_y++){
          if(max<shower[layer][num_z][num_y][0]){
            max = shower[layer][num_z][num_y][0];
          }
        }
      }
    }
  }
  return max;
}

void MVA_processing_formatting_normalization(){
  /**
  Take as input the dataset created by MVA_processing and normalize it.
  */
  ROOT::EnableImplicitMT(); // Tell ROOT you want to go parallel

  const char *input="data_MVA_24pixel.root";
  TChain *h = new TChain("h");
  h->Add(input);

  double shower_0[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1];
  TBranch *b_shower, *b_en_in, *b_pid, *b_en_mis;
  double en_in_0, en_mis_0;
  int pid_0;
  h->SetBranchAddress("primary", &pid_0, &b_pid);
  h->SetBranchAddress("en_in", &en_in_0, &b_en_in);
  h->SetBranchAddress("en_mis", &en_mis_0, &b_en_mis);
  h->SetBranchAddress("shower", shower_0, &b_shower);

  TFile *file = new TFile("data_MVA_24pixel_parte1.root", "recreate");
  TTree *tree = new TTree("h","ttree");
  int evt; // id of event from Geant4 simulation
  int primary;
  double theta, phi, en_in, en_mis;
  tree->Branch("evt",&evt,"evt/I");
  tree->Branch("primary",&primary, "primary/I");
  tree->Branch("en_in", &en_in, "en_in/D");
  tree->Branch("en_mis", &en_mis, "en_mis/D");
  //tree->Branch("theta", &theta, "theta/D");
  //tree->Branch("phi", &phi, "phi/D");
  double shower[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1];
  char shower_name[50];
  sprintf(shower_name, "shower[%d][%d][%d][1]/D", NUMBER_OF_LAYERS,
          NUMBER_OF_PIXEL_Z,NUMBER_OF_PIXEL_Y);
  tree->Branch("shower", &shower, shower_name);

  double max = MVA_processing_normalization();

  for(evt=0; evt<h->GetEntries(); evt++){
    cout<<evt<<endl;
    h->GetEntry(evt);
    tree->GetEntry(evt);
    primary = pid_0;
    en_in = en_in_0;
    en_mis = en_mis_0;
    for(int layer=0; layer<NUMBER_OF_LAYERS; layer++){
      for(int num_z=0; num_z<NUMBER_OF_PIXEL_Z;num_z++){
        for(int num_y=0; num_y<NUMBER_OF_PIXEL_Y;num_y++){
          if(shower_0[layer][num_z][num_y][0]==-5){
            shower[layer][num_z][num_y][0] = -1.;
          }
          else{
            shower[layer][num_z][num_y][0] = shower_0[layer][num_z][num_y][0]/max;
          }
        }
      }
    }
    tree->Fill();
  }
  cout<<"****************************OK****************************"<<endl;
  tree->Write("", TObject::kWriteDelete); // write tree, then delete previous
  file->Close(); // close output file

}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

TH2D *set_hist_layer(const int LAYER, double shower[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1]){
  /**
  Fills an histogram of the layer #LAYER using the contents of shower 3Dx1 vector
  */
  char label[50];
  sprintf(label, "layer %d;y[mm];z[mm]", LAYER);

  TH2D *layer_x = new TH2D("",label, NUMBER_OF_PIXEL_Z,-200,200,NUMBER_OF_PIXEL_Y,-200,200);
  for(int num_z=0; num_z<NUMBER_OF_PIXEL_Z;num_z++){
    for(int num_y=0; num_y<NUMBER_OF_PIXEL_Y;num_y++){
      layer_x->SetBinContent(num_z+1, num_y+1, shower[LAYER-1][num_z][num_y][0]);
      //std::cout<< LAYER-1 <<"\t"<< num_z <<"\t"<< num_y <<"\t"<< layer_x->GetBinContent(num_z, num_y)<<std::endl;
    }
  }
  return layer_x;
}

void do_stuff(TCanvas *c, int index, TH2D *hist){
  /**
  Plots the histogram inside a canvas
  */
  c->cd(index);
  //gPad->SetLogz();
  hist->SetContour(99);
  //hist->SetMaximum(1); hist->SetMinimum(-1);
  hist->Draw("COLZ");
}

void event_display(int const evento=0, Bool_t show_display = kFALSE){
  /**
  Displays an event of the simulation.
  Inputs:
  evento = number of the event to be displayed;
  show_display = default is kTRUE.

  Computes the energy deposited by the primary particle and confronts it to the
  initial energy;
  */

  ROOT::EnableImplicitMT(); // Tell ROOT you want to go parallel

  const char *input="data_MVA_24pixel_parte2.root";
  // const char *input = "../gan_data/data_GAN_shower.root";
  TChain *h = new TChain("h");
  h->Add(input);

  double en_calc=0;
  double shower[NUMBER_OF_LAYERS][NUMBER_OF_PIXEL_Z][NUMBER_OF_PIXEL_Y][1];
  TBranch *b_shower, *b_en_in, *b_pid, *b_en_mis;
  double en_in, en_mis;
  int pid;
  h->SetBranchAddress("primary", &pid, &b_pid);
  h->SetBranchAddress("en_in", &en_in, &b_en_in);
  h->SetBranchAddress("en_mis", &en_mis, &b_en_mis);
  h->SetBranchAddress("shower", shower, &b_shower);

  h->GetEntry(evento);

  vector<TH2D*> layer(NUMBER_OF_LAYERS);
  for(int i=0; i<NUMBER_OF_LAYERS;i++){
    layer[i] = set_hist_layer(i+1, shower);
    for(int num_z=0; num_z<NUMBER_OF_PIXEL_Z;num_z++){
      for(int num_y=0; num_y<NUMBER_OF_PIXEL_Y;num_y++){
        en_calc += TMath::Power(10,shower[i][num_z][num_y][0]*6.7404)/1E6;
      }
    }
  }

  if(show_display){
    TCanvas *c = new TCanvas("","",1200,800);
    int x_div = int(TMath::Sqrt(NUMBER_OF_LAYERS))+1;
    int y_div;
    if(x_div*(x_div-1)==NUMBER_OF_LAYERS){y_div=x_div-1;}
    else {y_div = x_div;}
    c->Divide(x_div,y_div);
    gStyle->SetOptStat(kFALSE);
    for(int i=0; i<NUMBER_OF_LAYERS; i++){
      do_stuff(c, i+1, layer[i]);
    }
  }

  // std::cout<<"Primary particle:\t" ;
  // switch (pid) {
  //       case -1 :
  //           std::cout<<"Positron"<<std::endl ;
  //           break;
  //       case 0 :
  //           std::cout<<"Photon"<<std::endl ;
  //           break;
  //       case 1 :
  //           std::cout<<"Electron"<<std::endl ;
  //           break;
  //   }
  cout<<"Initial energy: \t"<<en_in/1E6<<" GeV"<<endl;
  cout<<"Deposited energy:\t"<<en_mis/1E6<<" GeV"<<endl;
  cout<<"Recalculed energy:\t"<<en_calc<<" GeV"<<endl;
}
