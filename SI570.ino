//lvds
//#define FREQ_XTAL (114285267)
//cmos si570
#define FREQ_XTAL (114249860)
const byte MASTER_I2C_SDA = 4;
const byte MASTER_I2C_SCL = 5;


unsigned char si570_i2c_address = 0x55;
unsigned char dco_reg[13], dco_status='s';
unsigned long bitval[38];
unsigned long f_center=0, frequency=14200000, dco_freq=0;
unsigned int hs, n1;


//LiquidCrystal lcd(13, 12, 11, 10, 9, 8);
int count = 0;
char b[20], c[20];

/* tuning pot stuff */
unsigned char refreshDisplay = 0;
unsigned int stepSize = 100;
int tuningPosition = 0;

/* dds ddschip(DDS9850, 5, 6, 7, 125000000LL); */

void  printLine1(char *text){
  Serial.println(text);
}

void printLine2(char *text){
  Serial.println(text);
}

void displayFrequency(unsigned long f){
  int mhz, khz, hz;
  
  mhz = f / 1000000l;
  khz = (f % 1000000l)/1000;
  hz = f % 1000l;
  sprintf(b, "[%02d.%03d.%03d]", mhz, khz, hz);
  printLine1(b);
}

/* si570 related routines */
/* modes */
unsigned char isManual = 1;
unsigned ritOn = 0;


/* 
IMPORTANT, the wire.h is modified so that the internal pull up resisters are not enabled. 
This is required to interface Arduino with the 3.3v Si570's I2C interface.
routines to interface with si570 via i2c (clock on analog pin 5, data on analog pin 4) */

void i2c_write (char slave_address,char reg_address, char data )  {
  int rdata = data;
  Wire.beginTransmission(slave_address);
  Wire.write(reg_address);
  Wire.write(rdata);
  Wire.endTransmission();
}

char i2c_read ( char slave_address, int reg_address ) {
  unsigned char rdata = 0xFF;
  Wire.beginTransmission(slave_address);
  Wire.write(reg_address);
  Wire.endTransmission();
  Wire.requestFrom(slave_address,1);
  if (Wire.available()) rdata = Wire.read();
  return rdata;
}

void read_si570(){
  //we have to read eight consecutive registers starting at register 5
  for (int i = 7; i <= 12; i++) 
    dco_reg[i] = i2c_read( si570_i2c_address, i);
}

void write_si570()
{
  int idco, i;
  
  // Freeze DCO
  idco = i2c_read( si570_i2c_address,137);
  i2c_write(si570_i2c_address, 137, idco | 0x10 );
  	
  i2c_write(si570_i2c_address, 7, dco_reg[7]);
  
  //Set Registers
  for( i=7; i <= 12; i++){
    i2c_write(si570_i2c_address, i, dco_reg[i]);
    idco = i2c_read( si570_i2c_address, i);
  }

  // Unfreeze DCO
  idco = i2c_read( si570_i2c_address, 137 );
  i2c_write (si570_i2c_address, 137, idco & 0xEF );
  
  // Set new freq
  i2c_write(si570_i2c_address,135,0x40);        
}

void qwrite_si570()
{   
  int i, idco;
  //Set Registers
  for( i=8; i <= 12; i++){
    i2c_write(si570_i2c_address, i, dco_reg[i]);
    idco = i2c_read( si570_i2c_address, i);
  }
}

void setBitvals(void){

	//set the rfreq values for each bit of the rfreq (integral)
	bitval[28] = (FREQ_XTAL) / (hs * n1);
	bitval[29] = bitval[28] << 1;
	bitval[30] = bitval[29] << 1;
	bitval[31] = bitval[30] << 1;
	bitval[32] = bitval[31] << 1;
	bitval[33] = bitval[32] << 1;
	bitval[34] = bitval[33] << 1;
	bitval[35] = bitval[34] << 1;
	bitval[36] = bitval[35] << 1;
	bitval[37] = bitval[36] << 1;

	//set the rfreq values for each bit of the rfreq (integral)
	bitval[27] = bitval[28] >> 1;
	bitval[26] = bitval[27] >> 1;
	bitval[25] = bitval[26] >> 1;
	bitval[24] = bitval[25] >> 1;
	bitval[23] = bitval[24] >> 1;
	bitval[22] = bitval[23] >> 1;
	bitval[21] = bitval[22] >> 1;
	bitval[20] = bitval[21] >> 1;
	bitval[19] = bitval[20] >> 1;
	bitval[18] = bitval[19] >> 1;
	bitval[17] = bitval[18] >> 1;
	bitval[16] = bitval[17] >> 1;
	bitval[15] = bitval[16] >> 1;
	bitval[14] = bitval[15] >> 1;
	bitval[13] = bitval[14] >> 1;
	bitval[12] = bitval[13] >> 1;
	bitval[11] = bitval[12] >> 1;
	bitval[10] = bitval[11] >> 1;
	bitval[9] = bitval[10] >> 1;
	bitval[8] = bitval[9] >> 1;
	bitval[7] = bitval[8] >> 1;
	bitval[6] = bitval[7] >> 1;
	bitval[5] = bitval[6] >> 1;
	bitval[4] = bitval[5] >> 1;
	bitval[3] = bitval[4] >> 1;
	bitval[2] = bitval[3] >> 1;
	bitval[1] = bitval[2] >> 1;
	bitval[0] = bitval[1] >> 1;
}

//select reasonable dividers for a frequency
//in order to avoid overflow, the frequency is scaled by 10
void setDividers (unsigned long f){
  int i, j;
  unsigned long f_dco;
  
  for (i = 2; i <= 127; i+= 2)
    for (j = 4; j <= 11; j++){
      //skip 8 and 10 as unused
      if (j == 8 || j == 10)
        continue;
      f_dco = (f/10) * i * j;
      if (480000000L < f_dco && f_dco < 560000000L){
        if (hs != j || n1 != i){
          hs = j; n1 = i;
	  setBitvals();
        }
        //f_dco = fnew/10 * n1 * hs;
        return;
    }
  }
}

void setRfreq (unsigned long fnew){
  int i, bit, ireg, byte;
  unsigned long rfreq;

  //reset all the registers
  for (i = 7; i <= 12; i++)
    dco_reg[i] = 0;

  //set up HS
  dco_reg[7] = (hs - 4) << 5;
  dco_reg[7] = dco_reg[7] | ((n1 - 1) >> 2);
  dco_reg[8] = ((n1-1) & 0x3) << 6;

  ireg = 8; //registers go from 8 to 12 (five of them)
  bit = 5; //the bits keep walking down
  byte = 0;
  rfreq = 0;
  for (i = 37; i >= 0; i--){
    //skip if the bitvalue is set to zero, it means, we have hit the bottom of the bitval table
    if (bitval[i] == 0)
      break;

    if (fnew >= bitval[i]){
      fnew = fnew - bitval[i];
      byte = byte | (1 << bit);
    }
    //else{
    // putchar('0');
    //}

    bit--;
    if (bit < 0){
      bit = 7;
      //use OR instead of = as register[7] has N1 bits already set into it
      dco_reg[ireg] |= byte;
      byte = 0;
      ireg++;
    }
  }
}

void si570_tune(unsigned long newfreq){
  
  //check that we are not wasting our time here
  if (dco_freq == newfreq)
    return;
  
  //if the jump is small enough, we don't have to fiddle with the dividers
  if ((newfreq > f_center && newfreq - f_center < 50000L) ||
    (f_center > newfreq && f_center - newfreq < 50000L)){
    setRfreq(newfreq);
    dco_freq = newfreq;
    qwrite_si570();
    return;
  }
  //else it is a big jump
  setDividers(newfreq);
  setRfreq(newfreq);
  f_center = dco_freq = newfreq;
  write_si570();
}

void si570_init(){

	Serial.printf("1");
  // Force Si570 to reset to initial freq
  i2c_write(si570_i2c_address,135,0x01);
	Serial.printf("2");
  delay(20);
  read_si570();  
	Serial.printf("3");
}
