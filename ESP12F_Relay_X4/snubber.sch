EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title ""
Date ""
Rev ""
Comp ""
Comment1 ""
Comment2 ""
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L Relay:G5V-1 K?
U 1 1 60583B1B
P 3900 2500
F 0 "K?" V 3333 2500 50  0001 C CNN
F 1 "Relay" V 3425 2500 50  0000 C CNN
F 2 "Relay_THT:Relay_SPDT_Omron_G5V-1" H 5030 2470 50  0001 C CNN
F 3 "http://omronfs.omron.com/en_US/ecb/products/pdf/en-g5v_1.pdf" H 3900 2500 50  0001 C CNN
	1    3900 2500
	0    1    1    0   
$EndComp
Wire Wire Line
	3600 2700 3250 2700
$Comp
L Device:R R?
U 1 1 60588077
P 4300 2950
F 0 "R?" H 4370 2996 50  0001 L CNN
F 1 "R" H 4370 2950 50  0000 L CNN
F 2 "" V 4230 2950 50  0001 C CNN
F 3 "~" H 4300 2950 50  0001 C CNN
	1    4300 2950
	1    0    0    -1  
$EndComp
$Comp
L Device:C C?
U 1 1 60588CBE
P 4300 3250
F 0 "C?" H 4415 3296 50  0001 L CNN
F 1 "C" H 4415 3250 50  0000 L CNN
F 2 "" H 4338 3100 50  0001 C CNN
F 3 "~" H 4300 3250 50  0001 C CNN
	1    4300 3250
	1    0    0    -1  
$EndComp
Wire Wire Line
	4550 2800 4550 2900
Wire Wire Line
	4550 3300 4550 3400
Text Notes 2900 2800 0    50   ~ 0
230V AC
Connection ~ 4300 2800
Wire Wire Line
	4300 2800 4200 2800
Connection ~ 4300 3400
Wire Wire Line
	4300 3400 3600 3400
Wire Wire Line
	4300 2800 4550 2800
Wire Wire Line
	4300 3400 4550 3400
$Comp
L Device:Transformer_1P_1S 24V~
U 1 1 60582163
P 4950 3100
F 0 "24V~" H 5450 3100 50  0001 C CNN
F 1 "Transformer" H 4950 3390 50  0000 C CNN
F 2 "" H 4950 3100 50  0001 C CNN
F 3 "~" H 4950 3100 50  0001 C CNN
	1    4950 3100
	1    0    0    -1  
$EndComp
Text Label 3250 2700 0    50   ~ 0
N
Text Label 3250 2850 0    50   ~ 0
L
Wire Wire Line
	3600 3400 3600 2850
Wire Wire Line
	3600 2850 3250 2850
$EndSCHEMATC
