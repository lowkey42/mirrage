#include <iostream>
#include <cmath>

using namespace std;

double delta_L_t(double La) {
	// Table 1 of "A Visibility Matching Tone Reproduction Operator for High Dynamic Range Scenes"
	double x = log(La)/log(10); // to log_10
	double r = 0;

	if     (x<=-3.94  ) r = -2.86;
	else if(x<=-1.44  ) r = pow(0.405*x+1.6, 2.18) - 2.86;
	else if(x<=-0.0184) r = x - 0.395;
	else if(x<=1.9    ) r = pow(0.249*x+0.65, 2.7) - 0.72;
	else                r = x - 1.255;

	return pow(10.0, r);
}

double R(double L, double s) {
    return L / (L+s);
}

int main()
{
    auto range = pow(10.0, 6.0) - pow(10.0, -4.0);
    auto bin_size = range / 256.0;
    
    auto min_diff = 99999.0;
    
    for(int i=0; i<256; i++) {
        float L = bin_size*i + pow(10.0, -4.0);
        
        auto diff = abs(R(L,L) - R(L+delta_L_t(L), L));
        min_diff = min(min_diff, diff);
    }
 
    std::cout<<"DeltaR = "<<min_diff<<"\n";
}

