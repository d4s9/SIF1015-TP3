#include <stdio.h>
#include <math.h>

int main () {
   FILE *fp;
   FILE *fpBin;
   int i,c;
   unsigned short sum=0;
  
   fp = fopen("Mult-NumbersErreur.bin","r");
   fpBin = fopen("Mult-NumbersErreur.olc3","wb");
   if(fp == NULL) {
      printf("Error in opening file");
      return(-1);
   }
   fseek(fp, 0, SEEK_END);
   long nbcar = ftell(fp);
   printf("nbcar = %d\n", nbcar);
   fseek(fp,0,SEEK_SET);
   
   while(nbcar) {
   	c = fgetc(fp);
   	if(!(c=='0' || c == '1')){
   		nbcar--;
		   printf("nbcar = %d\n", nbcar);	 
      }
      else{
        sum = 0;
		  i = 0;
		  while(i < 16){
		      /*if( feof(fp) ) { 
		         break ;
		      }*/
		     if(c=='0' || c == '1'){ 
	            sum += (unsigned short)((c-30)*pow(2.0,15-i));
			    i++;   
	         }
	         nbcar--;
			   printf("nbcar = %d\n", nbcar);
			   c = fgetc(fp);	 
	      }
	      //printf("sum = %d \n", sum);
		  fwrite(&sum, 2, 1, fpBin);
		}
   }
   fclose(fp);
   fclose(fpBin);
   
   return(0);
}