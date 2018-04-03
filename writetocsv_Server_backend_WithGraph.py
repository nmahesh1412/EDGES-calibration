###############################################################################

#Clear All variables

def clearall():

    all = [var for var in globals() if var[0] != "_"]

    for var in all:

        del globals()[var]

clearall()

###############################################################################



from Tkinter import*

import u6,os,time,csv,datetime,math ,numpy as np,matplotlib.pyplot as plt



#os.system('clear')

###############################################################################
root = Tk()
d = u6.U6()
#d.configIO(FIOAnalog = 127) #AIN0-4 are analog inputs, 5-8 are digital (b00011111)
#d.getCalibrationData()
#EIO0_STATE_REGISTER = 6008
#EIO1_STATE_REGISTER = 6009
#EIO2_STATE_REGISTER = 6010
#EIO3_STATE_REGISTER = 6011
#EIO4_STATE_REGISTER = 6012
#EIO5_STATE_REGISTER = 6013
#EIO6_STATE_REGISTER = 6014
#EIO7_STATE_REGISTER = 6015
#def EIO(m,n):
    	#d.writeRegister(eval(str('EIO'+ str(m)+'_STATE_REGISTER')), n)
   	#print("Setting EIO# " + str(m) + " To : " + str(n))
	#for i in range(8):
	#	EIO(i,1)

now = datetime.datetime.now()
year=now.year
month=now.month
day=now.day
hour=now.hour
minute=now.minute
second=now.second
date= str(month) + "_" + str(day) + "_" + str(year)
time= str(hour) + "_" + str(minute) + "_" + str(second)
loadname = raw_input('Enter Load Name: ')
filename = str(loadname) +"_" + date + "_" + time + ".csv"
#root = Tk()
with open(filename, 'w') as csvfile:
	fieldnames = ['Date','Time','LNA Voltage','LNA Thermistor (Kohm)','LNA (C)','SP4T Voltage', 'SP4T Thermistor (Kohm)','SP4T (C)','Load Voltage','Load-thermistor (Kohm)','Load (C)','Internal_Temp(C)']
	writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
	writer.writeheader()
	#Leroy additions
	internalTlist = list() #adding array for plotting internal temperature
	timelist = list() #adding array for plotting time


	while True:
		now = datetime.datetime.now()
		year=now.year
		month=now.month
		day=now.day
		hour=now.hour
		minute=now.minute
		second=now.second
		date= str(month) + "/" + str(day) + "/" + str(year)
		time= str(hour) + ":" + str(minute) + ":" + str(second)
		timelist.append(second) #adding array for plotting time
		x = second
		internalT = d.getTemperature()-273
		internalTlist.append(internalT) #adding array for plotting 
		LNAresistance = d.getAIN(3)
		LNAV = LNAresistance
		SP4Tresistance = d.getAIN(0)
		SP4TV = SP4Tresistance
		Loadresistance = d.getAIN(1)
		LoadV = Loadresistance
		y1 = LNAresistance
		#y2= SP4Tresistance
		#y3= Loadresistance
		LNAresistance = ((LNAresistance*9918)/(5.05-LNAresistance))
		SP4Tresistance = ((SP4Tresistance*9960)/(4.9262-SP4Tresistance))
		Loadresistance = ((Loadresistance*9923)/(4.9262-Loadresistance))
		F1 = .00129675
		F2 = .000197374
		F3 = .000000304
		LNAdegc = 1/(F1+F2*math.log(LNAresistance)+F3*math.pow(math.log(LNAresistance),3))-273
		SP4Tdegc = 1/(F1+F2*math.log(SP4Tresistance)+F3*math.pow(math.log(SP4Tresistance),3))-273
		Loaddegc = -24.19*math.log(Loadresistance)+260.81
		
		writer.writerow({'Date': date,'Time': time,'LNA Voltage':LNAV,'LNA Thermistor (Kohm)': LNAresistance,'LNA (C)':LNAdegc,'SP4T Voltage': SP4TV, 'SP4T Thermistor (Kohm)': SP4Tresistance,'SP4T (C)':SP4Tdegc,'Load Voltage': LoadV,'Load-thermistor (Kohm)': Loadresistance,'Load (C)':Loaddegc,'Internal_Temp(C)':internalT})
		root.after(1000)
		print(date, time, round(LNAV,3), round(LNAresistance,0), round(LNAdegc,1), round(SP4TV,3), round(SP4Tresistance,0), round(SP4Tdegc,1), round(LoadV,3), round(Loadresistance,0), round(Loaddegc,1),internalT)		
		
	#red dashes, blue squares and green triangles
		#plt.autoscale(enable=True, axis='both', tight=False)
##		plt.plot(time,internalT,'r_')
		plt.xlabel('Time')
		plt.ylabel('Temperature')
		plt.plot(timelist,internalTlist,'r-')				
		#plt.tight_layout()
		#plt.plot(time,internalT, 'b*')
		#plt.plot(time,internalT,'g*')
		#plt.gcf().autofmt_xdate()
		plt.ion()
		plt.ticklabel_format(useOffset=False)
		plt.show(block=False)
		#plt.show()	
		plt.pause(.001)
		if second ==30:
			#timelist = list.clear()
			#internalTlist.append(internalT) = list()
			plt.clf
			plt.close()
		
		

   
