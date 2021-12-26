/*
This source is published under the following 3-clause BSD license.

Copyright (c) 2012 - 2013, Lukas Hosek and Alexander Wilkie
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * None of the names of the contributors may be used to endorse or promote 
      products derived from this software without specific prior written 
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/* ============================================================================

This file is part of a sample implementation of the analytical skylight and
solar radiance models presented in the SIGGRAPH 2012 paper


           "An Analytic Model for Full Spectral Sky-Dome Radiance"

and the 2013 IEEE CG&A paper

       "Adding a Solar Radiance Function to the Hosek Skylight Model"

                                   both by 

                       Lukas Hosek and Alexander Wilkie
                Charles University in Prague, Czech Republic


                        Version: 1.4a, February 22nd, 2013
                        
Version history:

1.4a  February 22nd, 2013
      Removed unnecessary and counter-intuitive solar radius parameters 
      from the interface of the colourspace sky dome initialisation functions.

1.4   February 11th, 2013
      Fixed a bug which caused the relative brightness of the solar disc
      and the sky dome to be off by a factor of about 6. The sun was too 
      bright: this affected both normal and alien sun scenarios. The 
      coefficients of the solar radiance function were changed to fix this.

1.3   January 21st, 2013 (not released to the public)
      Added support for solar discs that are not exactly the same size as
      the terrestrial sun. Also added support for suns with a different
      emission spectrum ("Alien World" functionality).

1.2a  December 18th, 2012
      Fixed a mistake and some inaccuracies in the solar radiance function
      explanations found in ArHosekSkyModel.h. The actual source code is
      unchanged compared to version 1.2.

1.2   December 17th, 2012
      Native RGB data and a solar radiance function that matches the turbidity
      conditions were added.

1.1   September 2012
      The coefficients of the spectral model are now scaled so that the output
      is given in physical units: W / (m^-2 * sr * nm). Also, the output of the
      XYZ model is now no longer scaled to the range [0...1]. Instead, it is
      the result of a simple conversion from spectral data via the CIE 2 degree
      standard observer matching functions. Therefore, after multiplication
      with 683 lm / W, the Y channel now corresponds to luminance in lm.
     
1.0   May 11th, 2012
      Initial release.


Please visit http://cgg.mff.cuni.cz/projects/SkylightModelling/ to check if
an updated version of this code has been published!

============================================================================ */


/*

This code is taken from ART, a rendering research system written in a
mix of C99 / Objective C. Since ART is not a small system and is intended to 
be inter-operable with other libraries, and since C does not have namespaces, 
the structures and functions in ART all have to have somewhat wordy 
canonical names that begin with Ar.../ar..., like those seen in this example.

Usage information:
==================


Model initialisation
--------------------

A separate ArHosekSkyModelState has to be maintained for each spectral
band you want to use the model for. So in a renderer with 'num_channels'
bands, you would need something like

    ArHosekSkyModelState  * skymodel_state[num_channels];

You then have to allocate and initialise these states. In the following code
snippet, we assume that 'albedo' is defined as

    double  albedo[num_channels];

with a ground albedo value between [0,1] for each channel. The solar elevation  
is given in radians.

    for ( unsigned int i = 0; i < num_channels; i++ )
        skymodel_state[i] =
            arhosekskymodelstate_alloc_init(
                  turbidity,
                  albedo[i],
                  solarElevation
                );

Note that starting with version 1.3, there is also a second initialisation 
function which generates skydome states for different solar emission spectra 
and solar radii: 'arhosekskymodelstate_alienworld_alloc_init()'.

See the notes about the "Alien World" functionality provided further down for a 
discussion of the usefulness and limits of that second initialisation function.
Sky model states that have been initialised with either function behave in a
completely identical fashion during use and cleanup.

Using the model to generate skydome samples
-------------------------------------------

Generating a skydome radiance spectrum "skydome_result" for a given location
on the skydome determined via the angles theta and gamma works as follows:

    double  skydome_result[num_channels];

    for ( unsigned int i = 0; i < num_channels; i++ )
        skydome_result[i] =
            arhosekskymodel_radiance(
                skymodel_state[i],
                theta,
                gamma,
                channel_center[i]
              );
              
The variable "channel_center" is assumed to hold the channel center wavelengths
for each of the num_channels samples of the spectrum we are building.


Cleanup after use
-----------------

After rendering is complete, the content of the sky model states should be
disposed of via

        for ( unsigned int i = 0; i < num_channels; i++ )
            arhosekskymodelstate_free( skymodel_state[i] );


CIE XYZ Version of the Model
----------------------------

Usage of the CIE XYZ version of the model is exactly the same, except that
num_channels is of course always 3, and that ArHosekTristimSkyModelState and
arhosek_tristim_skymodel_radiance() have to be used instead of their spectral
counterparts.

RGB Version of the Model
------------------------

The RGB version uses sRGB primaries with a linear gamma ramp. The same set of
functions as with the XYZ data is used, except the model is initialized
by calling arhosek_rgb_skymodelstate_alloc_init.

Solar Radiance Function
-----------------------

For each position on the solar disc, this function returns the entire radiance 
one sees - direct emission, as well as in-scattered light in the area of the 
solar disc. The latter is important for low solar elevations - nice images of 
the setting sun would not be possible without this. This is also the reason why 
this function, just like the regular sky dome model evaluation function, needs 
access to the sky dome data structures, as these provide information on 
in-scattered radiance.

CAVEAT #1: in this release, this function is only provided in spectral form!
           RGB/XYZ versions to follow at a later date.

CAVEAT #2: (fixed from release 1.3 onwards) 

CAVEAT #3: limb darkening renders the brightness of the solar disc
           inhomogeneous even for high solar elevations - only taking a single
           sample at the centre of the sun will yield an incorrect power
           estimate for the solar disc! Always take multiple random samples
           across the entire solar disc to estimate its power!
           
CAVEAT #4: in this version, the limb darkening calculations still use a fairly
           computationally expensive 5th order polynomial that was directly 
           taken from astronomical literature. For the purposes of Computer
           Graphics, this is needlessly accurate, though, and will be replaced 
           by a cheaper approximation in a future release.

"Alien World" functionality
---------------------------

The Hosek sky model can be used to roughly (!) predict the appearance of 
outdoor scenes on earth-like planets, i.e. planets of a similar size and 
atmospheric make-up. Since the spectral version of our model predicts sky dome 
luminance patterns and solar radiance independently for each waveband, and 
since the intensity of each waveband is solely dependent on the input radiance 
from the star that the world in question is orbiting, it is trivial to re-scale 
the wavebands to match a different star radiance.

At least in theory, the spectral version of the model has always been capable 
of this sort of thing, and the actual sky dome and solar radiance models were 
actually not altered at all in this release. All we did was to add some support
functionality for doing this more easily with the existing data and functions, 
and to add some explanations.

Just use 'arhosekskymodelstate_alienworld_alloc_init()' to initialise the sky
model states (you will have to provide values for star temperature and solar 
intensity compared to the terrestrial sun), and do everything else as you 
did before.

CAVEAT #1: we assume the emission of the star that illuminates the alien world 
           to be a perfect blackbody emission spectrum. This is never entirely 
           realistic - real star emission spectra are considerably more complex 
           than this, mainly due to absorption effects in the outer layers of 
           stars. However, blackbody spectra are a reasonable first assumption 
           in a usage scenario like this, where 100% accuracy is simply not 
           necessary: for rendering purposes, there are likely no visible 
           differences between a highly accurate solution based on a more 
           involved simulation, and this approximation.

CAVEAT #2: we always use limb darkening data from our own sun to provide this
           "appearance feature", even for suns of strongly different 
           temperature. Which is presumably not very realistic, but (as with 
           the unaltered blackbody spectrum from caveat #1) probably not a bad 
           first guess, either. If you need more accuracy than we provide here,
           please make inquiries with a friendly astro-physicst of your choice.

CAVEAT #3: you have to provide a value for the solar intensity of the star 
           which illuminates the alien world. For this, please bear in mind  
           that there is very likely a comparatively tight range of absolute  
           solar irradiance values for which an earth-like planet with an  
           atmosphere like the one we assume in our model can exist in the  
           first place!
            
           Too much irradiance, and the atmosphere probably boils off into 
           space, too little, it freezes. Which means that stars of 
           considerably different emission colour than our sun will have to be 
           fairly different in size from it, to still provide a reasonable and 
           inhabitable amount of irradiance. Red stars will need to be much 
           larger than our sun, while white or blue stars will have to be 
           comparatively tiny. The initialisation function handles this and 
           computes a plausible solar radius for a given emission spectrum. In
           terms of absolute radiometric values, you should probably not stray
           all too far from a solar intensity value of 1.0.

CAVEAT #4: although we now support different solar radii for the actual solar 
           disc, the sky dome luminance patterns are *not* parameterised by 
           this value - i.e. the patterns stay exactly the same for different 
           solar radii! Which is of course not correct. But in our experience, 
           solar discs up to several degrees in diameter (! - our own sun is 
           half a degree across) do not cause the luminance patterns on the sky 
           to change perceptibly. The reason we know this is that we initially 
           used unrealistically large suns in our brute force path tracer, in 
           order to improve convergence speeds (which in the beginning were 
           abysmal). Later, we managed to do the reference renderings much 
           faster even with realistically small suns, and found that there was 
           no real difference in skydome appearance anyway. 
           Conclusion: changing the solar radius should not be over-done, so  
           close orbits around red supergiants are a no-no. But for the  
           purposes of getting a fairly credible first impression of what an 
           alien world with a reasonably sized sun would look like, what we are  
           doing here is probably still o.k.

HINT #1:   if you want to model the sky of an earth-like planet that orbits 
           a binary star, just super-impose two of these models with solar 
           intensity of ~0.5 each, and closely spaced solar positions. Light is
           additive, after all. Tattooine, here we come... :-)

           P.S. according to Star Wars canon, Tattooine orbits a binary
           that is made up of a G and K class star, respectively. 
           So ~5500K and ~4200K should be good first guesses for their 
           temperature. Just in case you were wondering, after reading the
           previous paragraph.
*/


/*

This file contains the coefficient data for the spectral version of the model.
The original data has been reordered and reorganized for easier access

*/

#ifndef _SLG_ARHOSEKSKYMODELDATA_H
#define	_SLG_ARHOSEKSKYMODELDATA_H

namespace slg {

const float wavelengths[11] = {
320.f, 360.f, 400.f, 440.f, 480.f, 520.f, 560.f, 600.f, 640.f, 680.f, 720.f
};

const float datasets[11][2][10][10][6] = {
{ // 320.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-13.41049f, -14.26825f, -2.239068f, -1.336194f, -1.554271f, -0.8449639f
},
{ // coefficient B
-3.742293f, -3.550926f, -4.290407f, -0.2467808f, -1.811527f, -0.5665198f
},
{ // coefficient C
-5.229614f, 0.0571935f, -0.7494879f, 0.3961139f, 0.7309756f, 0.5525823f
},
{ // coefficient D
5.30718f, 0.3165753f, 0.2864989f, -0.0672382f, 0.001766793f, -0.00283887f
},
{ // coefficient E
-0.02182658f, -0.05870693f, -0.06017855f, -0.1817268f, 0.577909f, -4.555228f
},
{ // coefficient F
0.1497676f, 0.1333896f, 0.1325901f, 0.01017581f, 0.6186216f, 0.2824945f
},
{ // coefficient G
-8.56173e-06f, 1.779338e-05f, -0.0001661674f, 0.0006096079f, -0.001755338f, 0.004002014f
},
{ // coefficient H
1.73348f, 1.504276f, 1.73212f, 1.986859f, -0.0270109f, 1.114208f
},
{ // coefficient I
0.8826913f, 0.9750357f, 0.6513374f, 1.415296f, 0.269953f, 0.6637074f
},
{ // radiance
0.0009282016f, 0.0003169257f, 0.005255138f, -0.014652f, 0.07187172f, 0.0540086f
}
},
{ // turbidity 2
{ // coefficient A
-12.98333f, -14.02639f, -2.190256f, -2.415991f, -0.5254592f, -1.280108f
},
{ // coefficient B
-3.775577f, -3.787558f, -3.575495f, -1.453294f, -0.8181026f, -1.013716f
},
{ // coefficient C
-5.173531f, 0.07611941f, -0.4930996f, 0.2170671f, 0.7535702f, 0.5577676f
},
{ // coefficient D
5.316518f, 0.2521881f, 0.04826321f, 0.1341284f, -0.03323364f, 0.0009539205f
},
{ // coefficient E
-0.02572615f, -0.05859973f, -0.06797145f, -0.192633f, 0.4503149f, -4.934956f
},
{ // coefficient F
0.1516601f, 0.1753711f, 0.03425922f, 0.1059103f, 0.5778285f, 0.2642883f
},
{ // coefficient G
-8.297168e-06f, 4.670097e-05f, -0.000351255f, 0.001360739f, -0.004089673f, 0.01005169f
},
{ // coefficient H
1.669649f, 1.459275f, 1.978419f, 1.587725f, 0.3335089f, 0.9265844f
},
{ // coefficient I
0.9000495f, 0.8998629f, 0.8866517f, 0.9821154f, 0.6827164f, 0.4999698f
},
{ // radiance
0.0009160628f, 0.0002599956f, 0.005466998f, -0.01503537f, 0.07200167f, 0.05387713f
}
},
{ // turbidity 3
{ // coefficient A
-12.92247f, -14.33099f, -2.53922f, -2.393246f, -0.1185583f, -1.550325f
},
{ // coefficient B
-3.819777f, -3.766213f, -3.459074f, -1.937898f, -0.1960943f, -1.333575f
},
{ // coefficient C
-4.478733f, 0.493015f, -0.3774393f, 0.1005834f, 0.7212723f, 0.5618137f
},
{ // coefficient D
4.582924f, -0.03081235f, -0.3628419f, 0.586789f, -0.1763978f, 0.02563595f
},
{ // coefficient E
-0.0236437f, -0.06522199f, -0.2124451f, 0.2645044f, -1.00019f, -5.007716f
},
{ // coefficient F
0.1619828f, 0.1564198f, -0.01358132f, 0.1413695f, 0.6259726f, 0.06522985f
},
{ // coefficient G
-3.053548e-06f, 0.0003455657f, -0.001812805f, 0.006378716f, -0.01783726f, 0.0426283f
},
{ // coefficient H
1.646629f, 1.428507f, 2.245152f, 1.140715f, 0.7790644f, 0.737193f
},
{ // coefficient I
0.5103371f, 0.4312273f, 0.7247429f, 1.263014f, 0.324471f, 0.5239972f
},
{ // radiance
0.0009148749f, 0.0002164768f, 0.005576667f, -0.01537254f, 0.07215609f, 0.05380753f
}
},
{ // turbidity 4
{ // coefficient A
-12.34358f, -13.49727f, -3.399618f, -1.925152f, -0.5706832f, -1.371907f
},
{ // coefficient B
-3.851875f, -3.592681f, -3.818725f, -1.539333f, -0.6696186f, -1.14233f
},
{ // coefficient C
-3.911206f, 1.335192f, -0.8698171f, 0.2757771f, 0.6798158f, 0.5207805f
},
{ // coefficient D
4.011324f, -0.9426446f, 0.272393f, -0.0643598f, 0.6920162f, -0.004480298f
},
{ // coefficient E
-0.02734425f, -0.05741127f, -0.3644369f, 0.64667f, -3.898854f, -5.00895f
},
{ // coefficient F
0.1272306f, 0.09765267f, 0.1238759f, 0.03084382f, 0.5954021f, 0.1251549f
},
{ // coefficient G
-7.62821e-06f, 5.518099e-05f, -0.0005189179f, 0.00311473f, -0.01196667f, 0.03531514f
},
{ // coefficient H
1.661843f, 1.428554f, 2.279175f, 1.259818f, 0.5714991f, 0.8776759f
},
{ // coefficient I
0.8993903f, -0.4278471f, 1.841076f, 0.5121617f, 0.6528481f, 0.4999465f
},
{ // radiance
0.0009090685f, 0.000146784f, 0.00577587f, -0.01598491f, 0.0725253f, 0.0532987f
}
},
{ // turbidity 5
{ // coefficient A
-14.59738f, -12.23022f, -4.078262f, -2.499065f, 0.05456648f, -1.590269f
},
{ // coefficient B
-3.833562f, -3.942049f, -3.261096f, -2.408391f, -0.09221401f, -1.34433f
},
{ // coefficient C
-4.148717f, 1.183072f, -0.5520001f, 0.03391663f, 0.7403428f, 0.4805789f
},
{ // coefficient D
4.20327f, -0.9018678f, 0.2174261f, -0.06167543f, 0.5565324f, 0.5038509f
},
{ // coefficient E
-0.02484405f, -0.04644071f, -0.3582576f, 0.7555424f, -5.13497f, -3.370644f
},
{ // coefficient F
0.1189704f, 0.1237476f, 0.02000597f, 0.2349252f, 0.3021763f, 0.3040357f
},
{ // coefficient G
0.0004166397f, -0.002359994f, 0.009890182f, -0.0244314f, 0.036385f, 0.002418483f
},
{ // coefficient H
1.74885f, 1.471013f, 2.199274f, 1.32854f, 0.5560149f, 0.8979818f
},
{ // coefficient I
0.4999721f, 0.5298845f, 0.275632f, 1.348906f, 0.181821f, 0.7477974f
},
{ // radiance
0.000890283f, 0.0001126529f, 0.005945913f, -0.01648173f, 0.07220217f, 0.05391054f
}
},
{ // turbidity 6
{ // coefficient A
-6.77568f, -7.800595f, -4.392403f, -2.469701f, -0.7759612f, -0.996234f
},
{ // coefficient B
-3.436745f, -2.867058f, -3.857979f, -1.357319f, -1.298076f, -0.648873f
},
{ // coefficient C
-2.69673f, 1.478909f, -1.02202f, 0.21326f, 0.7162377f, 0.3933344f
},
{ // coefficient D
2.740681f, -1.38016f, 1.449394f, -1.918729f, 2.906682f, -0.004752111f
},
{ // coefficient E
-0.04032382f, -0.1658909f, 0.2769695f, -4.19306f, -0.8261148f, -4.721793f
},
{ // coefficient F
0.1036486f, 0.1962673f, -0.3331834f, 0.8101579f, -0.2892123f, 0.6053196f
},
{ // coefficient G
8.133034e-05f, -0.0006512798f, 0.00351395f, -0.009605279f, 0.01491449f, 0.003453563f
},
{ // coefficient H
1.76716f, 1.634359f, 1.942113f, 1.844443f, 0.06529387f, 1.247655f
},
{ // coefficient I
0.5401354f, 0.4300704f, 0.3970742f, 1.58231f, -0.04180287f, 0.8673379f
},
{ // radiance
0.0008885423f, 0.000114235f, 0.005938903f, -0.016688f, 0.07231405f, 0.05331532f
}
},
{ // turbidity 7
{ // coefficient A
-7.552689f, -8.494732f, -4.672972f, -2.256117f, -0.7790059f, -1.046916f
},
{ // coefficient B
-3.219112f, -3.138528f, -4.049529f, -1.258356f, -1.269213f, -0.6991719f
},
{ // coefficient C
-2.730242f, 1.424739f, -1.0276f, 0.2198377f, 0.6315194f, 0.3620624f
},
{ // coefficient D
2.755929f, -1.269326f, 1.072252f, -1.296239f, 2.36885f, 0.07364236f
},
{ // coefficient E
-0.03925138f, -0.156158f, 0.07908165f, -3.20097f, -1.199163f, -5.012491f
},
{ // coefficient F
0.08394617f, 0.176706f, -0.2243835f, 0.6407291f, -0.1504024f, 0.4240417f
},
{ // coefficient G
0.000151498f, -0.001175921f, 0.006190595f, -0.01527762f, 0.01733299f, 0.03580425f
},
{ // coefficient H
1.84441f, 1.659123f, 1.988822f, 1.735209f, 0.2544016f, 1.202329f
},
{ // coefficient I
0.5389194f, 0.3746132f, 0.6684758f, 1.17053f, 0.2756763f, 0.6255804f
},
{ // radiance
0.0008674766f, 3.506619e-05f, 0.006176212f, -0.01732036f, 0.07223472f, 0.05318228f
}
},
{ // turbidity 8
{ // coefficient A
-18.86851f, -19.86322f, 0.535566f, -3.136053f, 0.1496752f, -1.83974f
},
{ // coefficient B
-4.491136f, -3.528401f, -5.459304f, -0.2856938f, -0.9170428f, -1.27926f
},
{ // coefficient C
-3.66044f, 1.401749f, -0.8809226f, 0.104839f, 0.5628226f, 0.3173503f
},
{ // coefficient D
3.704226f, -1.191377f, 0.5959028f, -0.7708877f, 1.733601f, 1.096266f
},
{ // coefficient E
-0.03158478f, -0.07474944f, -0.3311339f, 0.210663f, -4.784033f, -4.168649f
},
{ // coefficient F
0.1229909f, -0.02193835f, 0.3876731f, -0.1488471f, 0.357033f, 0.2121881f
},
{ // coefficient G
0.0009233613f, -0.005138968f, 0.0212607f, -0.05172733f, 0.0739658f, 0.02150917f
},
{ // coefficient H
1.745459f, 1.710181f, 1.929868f, 1.769302f, 0.3954993f, 1.151497f
},
{ // coefficient I
0.5011929f, 0.5473672f, 0.1883429f, 1.526253f, 0.001397727f, 0.7233585f
},
{ // radiance
0.0008525095f, -1.752028e-05f, 0.006286417f, -0.01779286f, 0.07150222f, 0.05334072f
}
},
{ // turbidity 9
{ // coefficient A
-18.4109f, -19.47081f, 0.9588307f, -3.609766f, 0.6087707f, -2.148492f
},
{ // coefficient B
-4.803089f, -2.96126f, -6.379803f, -0.001823498f, -0.6744295f, -1.59789f
},
{ // coefficient C
-4.883823f, 1.963002f, -1.02109f, 0.02516424f, 0.423631f, 0.3166858f
},
{ // coefficient D
4.962235f, -1.942423f, 0.9560589f, -0.8909855f, 1.505925f, 2.414329f
},
{ // coefficient E
-0.02693216f, -0.05550118f, -0.21755f, 0.2207805f, -4.197126f, -4.201339f
},
{ // coefficient F
0.1089917f, 0.07036456f, 0.08613517f, 0.1783064f, 0.2046561f, 0.1005902f
},
{ // coefficient G
-6.338015e-06f, -0.0005990592f, 0.01362268f, -0.04293958f, 0.06445824f, 0.04687865f
},
{ // coefficient H
1.725865f, 1.807097f, 1.953529f, 1.607901f, 0.7793118f, 0.9335376f
},
{ // coefficient I
0.8890717f, -0.008871814f, 0.7422482f, 1.131559f, 0.3262213f, 0.5289661f
},
{ // radiance
0.0008232652f, -0.0001292152f, 0.00664527f, -0.01886566f, 0.07052974f, 0.05331726f
}
},
{ // turbidity 10
{ // coefficient A
-17.87767f, -19.32995f, 1.788482f, -3.363801f, 0.0004668756f, -1.759172f
},
{ // coefficient B
-4.873485f, -4.092647f, -5.652582f, -0.3266475f, -0.9612337f, -1.386072f
},
{ // coefficient C
-5.861224f, 2.679531f, -1.517519f, 0.1313912f, 0.3082344f, 0.2349661f
},
{ // coefficient D
5.866336f, -2.555671f, 1.251535f, -0.6889075f, 1.298938f, 1.592185f
},
{ // coefficient E
-0.01564202f, -0.04181418f, -0.2225912f, 0.4356523f, -4.371395f, -3.063267f
},
{ // coefficient F
0.0696764f, 0.1023654f, -0.1084716f, 0.3120297f, 0.08845524f, 0.1529736f
},
{ // coefficient G
0.0007574926f, -0.005964172f, 0.03272584f, -0.08700131f, 0.1303481f, 0.03429185f
},
{ // coefficient H
1.768065f, 1.625691f, 2.139078f, 1.750122f, 0.584372f, 1.030294f
},
{ // coefficient I
0.5856596f, 0.4036808f, 0.3876645f, 1.223546f, 0.2718863f, 0.6917018f
},
{ // radiance
0.0007670001f, -0.0001885989f, 0.006484739f, -0.01852036f, 0.06610758f, 0.05484068f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-13.41051f, -14.26829f, -2.239143f, -1.33635f, -1.554584f, -0.8456234f
},
{ // coefficient B
-3.742047f, -3.55066f, -4.290084f, -0.2463606f, -1.810948f, -0.5656772f
},
{ // coefficient C
-5.229556f, 0.05731266f, -0.7493158f, 0.3963632f, 0.7313475f, 0.5531782f
},
{ // coefficient D
5.307222f, 0.3166575f, 0.2865753f, -0.06721398f, 0.001703307f, -0.002975361f
},
{ // coefficient E
-0.02094796f, -0.0582108f, -0.05993923f, -0.1816244f, 0.5779436f, -4.555221f
},
{ // coefficient F
0.1499787f, 0.133586f, 0.1327393f, 0.01021503f, 0.618479f, 0.2821035f
},
{ // coefficient G
-7.023116e-06f, 0.0002003178f, -0.0005399344f, 0.000808147f, -0.0006832621f, 0.0004007713f
},
{ // coefficient H
1.732898f, 1.503788f, 1.731677f, 1.986453f, -0.02739619f, 1.11385f
},
{ // coefficient I
0.8826861f, 0.975031f, 0.6513329f, 1.415292f, 0.2699443f, 0.6636719f
},
{ // radiance
0.001105405f, 2.555979e-05f, 0.007984713f, -0.02152422f, 0.08452836f, 0.09622688f
}
},
{ // turbidity 2
{ // coefficient A
-12.63311f, -13.614f, -1.477014f, -2.050884f, -0.3704793f, -0.9877542f
},
{ // coefficient B
-4.099112f, -3.538236f, -3.66431f, -1.470536f, -0.5829841f, -0.7646946f
},
{ // coefficient C
-5.130792f, 0.1570583f, -0.1442116f, 0.332759f, 1.168589f, 0.8859003f
},
{ // coefficient D
5.526406f, 0.3445259f, -0.04293554f, 0.1976143f, -0.1387973f, 0.05263145f
},
{ // coefficient E
-0.02122841f, -0.05306874f, -0.03768326f, -0.1930369f, 0.5998426f, -4.84941f
},
{ // coefficient F
0.1202556f, 0.2322893f, -0.138853f, 0.283196f, 0.3825096f, 0.1301905f
},
{ // coefficient G
-8.06067e-06f, 1.572516e-05f, -0.0001687893f, 0.0006940849f, -0.002188637f, 0.005464872f
},
{ // coefficient H
1.209196f, 1.15275f, 1.592625f, 1.271203f, -0.2273264f, 0.5270984f
},
{ // coefficient I
0.8997967f, 0.9026902f, 0.8804841f, 0.9790242f, 0.7393514f, 0.5001226f
},
{ // radiance
0.001111427f, -6.046059e-05f, 0.008035207f, -0.02134768f, 0.08363698f, 0.09641842f
}
},
{ // turbidity 3
{ // coefficient A
-12.94452f, -14.25646f, -2.236186f, -1.991088f, -0.2733597f, -0.9782447f
},
{ // coefficient B
-3.858933f, -3.745266f, -3.560196f, -2.028027f, -0.193859f, -0.8452104f
},
{ // coefficient C
-4.362969f, 0.6901454f, -0.1053059f, 0.2747056f, 1.108079f, 0.9149995f
},
{ // coefficient D
4.657159f, -0.05345642f, -0.3002269f, 0.5076456f, -0.166808f, 0.02750126f
},
{ // coefficient E
-0.02281956f, -0.05238708f, -0.170275f, 0.2056096f, -0.81363f, -4.996263f
},
{ // coefficient F
0.1371565f, 0.1157055f, 0.03290753f, 0.153704f, 0.3962292f, 0.1226836f
},
{ // coefficient G
3.55261e-06f, -1.96111e-05f, -7.253274e-05f, 0.001010611f, -0.004757393f, 0.01533731f
},
{ // coefficient H
1.354369f, 1.030408f, 1.846963f, 0.8439763f, 0.1536882f, 0.3686265f
},
{ // coefficient I
0.5222053f, 0.4036292f, 0.6828f, 1.286657f, 0.4595467f, 0.5004484f
},
{ // radiance
0.001103552f, -8.22916e-05f, 0.008170699f, -0.0218994f, 0.08404871f, 0.09504912f
}
},
{ // turbidity 4
{ // coefficient A
-12.34416f, -13.49621f, -3.39195f, -1.915114f, -0.5674128f, -1.343751f
},
{ // coefficient B
-3.818503f, -3.551853f, -3.791028f, -1.49308f, -0.5568719f, -0.9647458f
},
{ // coefficient C
-3.804408f, 1.469377f, -0.6935352f, 0.4567342f, 0.9084369f, 0.7945561f
},
{ // coefficient D
4.093838f, -0.8971164f, 0.2998175f, -0.07334296f, 0.668487f, 0.0005127485f
},
{ // coefficient E
-0.02505623f, -0.05623731f, -0.3155309f, 0.676706f, -3.89272f, -5.009873f
},
{ // coefficient F
0.1149315f, 0.1078524f, 0.1196041f, -0.004562689f, 0.5312866f, 0.06165389f
},
{ // coefficient G
-7.610563e-06f, 0.001491378f, -0.004708048f, 0.01016838f, -0.01768715f, 0.02642926f
},
{ // coefficient H
1.361584f, 1.11304f, 2.001973f, 1.003272f, 0.3252051f, 0.6213595f
},
{ // coefficient I
0.8981571f, -0.4279059f, 1.840166f, 0.5102018f, 0.6541029f, 0.5327376f
},
{ // radiance
0.001075129f, -0.000147397f, 0.008391094f, -0.02255482f, 0.08415497f, 0.09338994f
}
},
{ // turbidity 5
{ // coefficient A
-14.50562f, -12.97772f, -1.991837f, -2.052725f, -0.3737447f, -1.002263f
},
{ // coefficient B
-4.021778f, -3.655039f, -3.919397f, -1.606839f, -0.4061843f, -1.006484f
},
{ // coefficient C
-4.181517f, 2.030636f, -0.8466357f, 0.5876624f, 0.8985804f, 0.9629219f
},
{ // coefficient D
4.509518f, -1.537436f, 0.5319508f, -0.07464148f, 0.5700187f, 0.08528366f
},
{ // coefficient E
-0.02369519f, -0.0491549f, -0.3165811f, 0.7525473f, -5.422882f, -4.220831f
},
{ // coefficient F
0.06955582f, 0.1511047f, 0.1056172f, 0.001590947f, 0.4334301f, 0.07848806f
},
{ // coefficient G
0.0001149882f, -0.0009053517f, 0.004836036f, -0.01261084f, 0.01759754f, 0.01254937f
},
{ // coefficient H
1.329583f, 1.15698f, 1.843574f, 0.8587516f, 0.2613056f, 0.2140208f
},
{ // coefficient I
0.5260857f, 0.483169f, 0.3084513f, 1.404076f, 0.2062463f, 0.6187813f
},
{ // radiance
0.001036467f, -0.0001328992f, 0.008348236f, -0.02256138f, 0.08304671f, 0.09263679f
}
},
{ // turbidity 6
{ // coefficient A
-7.175451f, -8.058121f, -4.387785f, -1.807473f, -0.6242455f, -0.8143405f
},
{ // coefficient B
-3.313094f, -3.119458f, -3.851081f, -1.498491f, -1.06385f, -0.5251732f
},
{ // coefficient C
-2.396914f, 1.729776f, -0.7774273f, 0.3237723f, 1.166897f, 0.7822692f
},
{ // coefficient D
2.657177f, -1.43537f, 1.40552f, -1.626737f, 2.283807f, -0.00376428f
},
{ // coefficient E
-0.03936959f, -0.1414745f, 0.1743075f, -3.741656f, -1.150947f, -5.011112f
},
{ // coefficient F
0.1123476f, 0.2338787f, -0.4108912f, 0.8444519f, -0.3242997f, 0.3985065f
},
{ // coefficient G
7.31586e-05f, -0.0005865288f, 0.003058571f, -0.008297013f, 0.0127934f, 0.003652394f
},
{ // coefficient H
1.431209f, 1.247225f, 1.519803f, 1.624861f, -0.5448139f, 0.7140115f
},
{ // coefficient I
0.5107145f, 0.5037743f, 0.2636805f, 1.753817f, -0.08653106f, 0.700491f
},
{ // radiance
0.001042383f, -0.0002016636f, 0.008479624f, -0.02296524f, 0.08303746f, 0.09082494f
}
},
{ // turbidity 7
{ // coefficient A
-7.579673f, -8.524117f, -4.579718f, -1.946704f, -0.7123367f, -0.7979833f
},
{ // coefficient B
-3.495594f, -3.121778f, -4.086739f, -1.512415f, -1.133848f, -0.554603f
},
{ // coefficient C
-2.49883f, 1.680395f, -0.8030175f, 0.3559506f, 1.0648f, 0.776585f
},
{ // coefficient D
2.77147f, -1.329455f, 1.10339f, -1.318829f, 2.217543f, -0.004676928f
},
{ // coefficient E
-0.03451351f, -0.1395912f, 0.1429699f, -3.063409f, -1.247373f, -5.014372f
},
{ // coefficient F
0.08186886f, 0.1911589f, -0.2211841f, 0.6250046f, -0.2105594f, 0.3694816f
},
{ // coefficient G
0.0001272079f, -0.001027988f, 0.005620349f, -0.01490672f, 0.02261048f, 0.002373221f
},
{ // coefficient H
1.39042f, 1.259267f, 1.693702f, 1.41452f, -0.3648618f, 0.6783145f
},
{ // coefficient I
0.5545117f, 0.3356989f, 0.7084432f, 1.317647f, 0.1147223f, 0.7862971f
},
{ // radiance
0.001014293f, -0.0002355927f, 0.00855101f, -0.02333261f, 0.0825816f, 0.08873588f
}
},
{ // turbidity 8
{ // coefficient A
-18.86599f, -19.85552f, 0.5631541f, -3.083195f, 0.2003824f, -1.738851f
},
{ // coefficient B
-4.523457f, -3.53004f, -5.483754f, -0.3071409f, -0.8374106f, -1.144728f
},
{ // coefficient C
-3.559445f, 1.578922f, -0.6832337f, 0.282236f, 0.8800366f, 0.6950894f
},
{ // coefficient D
3.748747f, -1.170838f, 0.6170499f, -0.7871125f, 1.702679f, 1.07387f
},
{ // coefficient E
-0.02082711f, -0.06347814f, -0.2824393f, 0.2420642f, -4.785357f, -4.176611f
},
{ // coefficient F
0.1120505f, 0.002032158f, 0.354681f, -0.1716096f, 0.3194078f, 0.09901497f
},
{ // coefficient G
4.908827e-05f, -0.0007291357f, 0.00624842f, -0.01936307f, 0.03028099f, 0.02719362f
},
{ // coefficient H
1.430333f, 1.399338f, 1.63852f, 1.450902f, 0.06649245f, 0.6579628f
},
{ // coefficient I
0.4999603f, 0.5515394f, 0.2300717f, 1.590291f, -0.004459174f, 0.6542088f
},
{ // radiance
0.0009683336f, -0.0002120256f, 0.008172046f, -0.02223973f, 0.07852279f, 0.08864017f
}
},
{ // turbidity 9
{ // coefficient A
-18.42858f, -19.4622f, 1.012726f, -3.502022f, 0.702455f, -2.056576f
},
{ // coefficient B
-4.464894f, -2.928492f, -6.397245f, -0.09795202f, -0.6903001f, -1.583471f
},
{ // coefficient C
-4.876181f, 1.932796f, -0.8570688f, 0.3286657f, 0.7519618f, 0.7947031f
},
{ // coefficient D
4.999894f, -2.009963f, 0.9466942f, -0.8318684f, 1.501909f, 2.224097f
},
{ // coefficient E
-0.01680473f, -0.02067172f, -0.2428074f, 0.1254695f, -4.224528f, -4.127138f
},
{ // coefficient F
0.09284371f, 0.2760643f, 0.04505376f, 0.09600123f, 0.1253867f, 0.1885249f
},
{ // coefficient G
8.631599e-05f, -0.001316034f, 0.01159119f, -0.03501772f, 0.05829515f, 1.729132e-05f
},
{ // coefficient H
1.551671f, 1.910005f, 1.686356f, 1.161309f, 0.2671626f, 0.3336881f
},
{ // coefficient I
0.8637202f, -0.02057018f, 0.7885028f, 1.205342f, 0.3030079f, 0.5332061f
},
{ // radiance
0.0009139571f, -0.0002688667f, 0.008068793f, -0.0222439f, 0.0759032f, 0.08464876f
}
},
{ // turbidity 10
{ // coefficient A
-18.25745f, -19.81828f, 1.346751f, -3.99632f, 0.4789259f, -1.772626f
},
{ // coefficient B
-5.122861f, -3.746331f, -6.486837f, -0.4646476f, -0.8952042f, -1.783205f
},
{ // coefficient C
-5.876715f, 2.729783f, -1.082622f, 0.03008639f, 0.8595191f, 0.7322534f
},
{ // coefficient D
5.970876f, -2.477317f, 1.125524f, -1.193861f, 2.353988f, 1.529827f
},
{ // coefficient E
-0.01788919f, -0.02797536f, -0.2228944f, 0.444396f, -4.885398f, -2.789303f
},
{ // coefficient F
0.1063934f, -0.05281985f, 0.3975587f, -0.2593178f, 0.4438339f, -0.181721f
},
{ // coefficient G
0.0004770037f, -0.003739083f, 0.02035973f, -0.05378377f, 0.07931163f, 0.02737502f
},
{ // coefficient H
1.612403f, 1.378246f, 1.884664f, 1.528777f, -0.08144393f, 0.2620216f
},
{ // coefficient I
0.5156506f, 0.5904024f, 0.001086023f, 1.734923f, 0.01663921f, 0.6227585f
},
{ // radiance
0.0008457855f, -0.0003459869f, 0.007626953f, -0.02067614f, 0.06841363f, 0.08244103f
}
}
}
},
{ // 360.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-2.97429f, -2.638402f, -2.449142f, -0.8559357f, -1.098145f, -1.124849f
},
{ // coefficient B
-1.670904f, -1.651876f, -1.541788f, -0.5054959f, -0.5123514f, -0.5693597f
},
{ // coefficient C
-5.183199f, -1.229939f, 0.4629358f, 0.4334205f, 0.7773196f, 0.7315125f
},
{ // coefficient D
5.377376f, 1.776358f, -1.149742f, 0.4254155f, -0.1325175f, 0.02986435f
},
{ // coefficient E
-0.02904124f, -0.06372414f, -0.07317185f, -0.2920605f, 0.4648396f, -4.536788f
},
{ // coefficient F
0.248672f, 0.3767834f, -0.2422158f, 0.930323f, 0.1386648f, 0.6650081f
},
{ // coefficient G
8.045624e-05f, -0.000647553f, 0.003667113f, -0.01212609f, 0.02427679f, -9.004215e-07f
},
{ // coefficient H
1.889212f, 1.373589f, 3.146429f, 0.4498482f, 1.199386f, 1.00638f
},
{ // coefficient I
0.5188203f, 0.4349252f, 0.6725657f, 0.2117838f, 0.7988611f, 0.4999682f
},
{ // radiance
0.002494129f, 0.003556297f, 0.0002965923f, 0.002713084f, 0.1335823f, 0.08293879f
}
},
{ // turbidity 2
{ // coefficient A
-2.709497f, -2.590279f, -2.988403f, -0.2932084f, -1.3794f, -1.042446f
},
{ // coefficient B
-1.635812f, -1.525236f, -1.938687f, -0.132391f, -0.6876864f, -0.5201193f
},
{ // coefficient C
-4.594177f, -0.6846073f, 0.04954649f, 0.553591f, 0.7224534f, 0.7107656f
},
{ // coefficient D
4.809336f, 1.21608f, -0.8992325f, 0.7456284f, -0.6172704f, 0.4182566f
},
{ // coefficient E
-0.03672907f, -0.08022814f, -0.07157111f, -0.27134f, 0.12175f, -4.956238f
},
{ // coefficient F
0.2383111f, 0.3040074f, -0.04370068f, 0.6989175f, 0.3074795f, 0.5315425f
},
{ // coefficient G
3.241989e-05f, -0.0004984576f, 0.004336645f, -0.01513162f, 0.02905167f, 0.006232999f
},
{ // coefficient H
1.885505f, 1.521429f, 2.849496f, 0.6926848f, 1.123563f, 1.059015f
},
{ // coefficient I
0.6619619f, 0.174604f, 0.8892788f, 0.5719944f, 0.683751f, 0.5116427f
},
{ // radiance
0.002473622f, 0.003518055f, 0.0004432438f, 0.001754027f, 0.1352516f, 0.08253805f
}
},
{ // turbidity 3
{ // coefficient A
-2.686687f, -3.04967f, -2.414334f, -0.6821703f, -1.227285f, -1.080553f
},
{ // coefficient B
-1.615137f, -1.775802f, -1.637863f, -0.3665215f, -0.5958479f, -0.5308084f
},
{ // coefficient C
-3.811956f, 0.02577719f, -0.3948523f, 0.7496076f, 0.5828611f, 0.6980027f
},
{ // coefficient D
4.018597f, 0.53312f, -0.4867989f, 0.539176f, -0.4050489f, 0.2193524f
},
{ // coefficient E
-0.03953418f, -0.09424429f, -0.1545516f, -0.290345f, -0.2935835f, -5.007206f
},
{ // coefficient F
0.2171778f, 0.3852753f, -0.2045592f, 0.8498544f, 0.1410603f, 0.5895807f
},
{ // coefficient G
-7.374887e-06f, -8.678326e-05f, 0.002308206f, -0.0138899f, 0.04352812f, 0.01122391f
},
{ // coefficient H
1.875745f, 1.347053f, 3.257409f, 0.2126622f, 1.406076f, 1.052433f
},
{ // coefficient I
0.899618f, -0.1078327f, 1.029606f, 0.6857005f, 0.4955059f, 0.6643198f
},
{ // radiance
0.002485307f, 0.003507686f, 0.0004235269f, 0.001120748f, 0.1360769f, 0.08328522f
}
},
{ // turbidity 4
{ // coefficient A
-2.983088f, -2.858023f, -2.858102f, -0.3305766f, -1.322728f, -1.117086f
},
{ // coefficient B
-1.746025f, -1.624623f, -2.072804f, -0.008277591f, -0.7101315f, -0.5760006f
},
{ // coefficient C
-4.564198f, 1.859497f, -1.205584f, 0.8561917f, 0.5645563f, 0.6388217f
},
{ // coefficient D
4.776145f, -1.348577f, 0.5674135f, -0.2239172f, 0.9897384f, 0.165929f
},
{ // coefficient E
-0.03208607f, -0.07427284f, -0.3934144f, 0.5896697f, -6.756374f, -4.687099f
},
{ // coefficient F
0.2008358f, 0.2943068f, -0.03736288f, 0.6229823f, 0.2491515f, 0.454454f
},
{ // coefficient G
-6.578048e-06f, -0.0001961205f, 0.004780332f, -0.02242577f, 0.05308124f, 0.02672063f
},
{ // coefficient H
1.82988f, 1.502334f, 2.831298f, 0.7216263f, 1.166258f, 1.067777f
},
{ // coefficient I
0.9001069f, -0.3183648f, 1.553942f, 0.4295956f, 0.5472501f, 0.6419825f
},
{ // radiance
0.002421491f, 0.003278595f, 0.001395344f, -0.001953245f, 0.1392978f, 0.08376885f
}
},
{ // turbidity 5
{ // coefficient A
-2.94334f, -3.717358f, -2.439542f, -0.1562453f, -1.579698f, -0.995961f
},
{ // coefficient B
-1.779161f, -1.850324f, -2.277701f, 0.5099893f, -1.110703f, -0.4119588f
},
{ // coefficient C
-3.715839f, 2.277659f, -1.702329f, 1.026847f, 0.4743061f, 0.5707529f
},
{ // coefficient D
3.949049f, -2.02779f, 2.025885f, -2.516874f, 4.160187f, -0.003505536f
},
{ // coefficient E
-0.04499824f, -0.124073f, -0.1142291f, -2.212969f, -4.597085f, -4.253949f
},
{ // coefficient F
0.2234466f, 0.3029522f, -0.2011641f, 0.925266f, -0.2112156f, 0.7498003f
},
{ // coefficient G
-8.091518e-06f, -0.0002176255f, 0.005563891f, -0.02696795f, 0.0679177f, 0.008246945f
},
{ // coefficient H
1.825217f, 1.585933f, 2.424957f, 1.443711f, 0.6131169f, 1.397857f
},
{ // coefficient I
0.9000118f, -0.2794001f, 1.399542f, 0.7450372f, 0.2291635f, 0.8021253f
},
{ // radiance
0.002403587f, 0.003114517f, 0.002224702f, -0.00530522f, 0.1435766f, 0.08315234f
}
},
{ // turbidity 6
{ // coefficient A
-2.304734f, -3.130477f, -1.110129f, -1.17454f, -1.104583f, -1.105893f
},
{ // coefficient B
-1.483605f, -1.966839f, -1.306323f, 0.07550161f, -0.8430389f, -0.4500399f
},
{ // coefficient C
-1.531059f, 0.60586f, -0.3235833f, -0.1849588f, 0.8668416f, 0.4286451f
},
{ // coefficient D
1.707306f, -0.9209976f, 2.998748f, -6.281432f, 9.153752f, 1.207864f
},
{ // coefficient E
-0.1150367f, -0.1932901f, -0.01535927f, -6.186665f, -0.728954f, -5.018291f
},
{ // coefficient F
0.2153063f, 0.3242645f, -0.4001906f, 1.173048f, -0.4346537f, 0.6988292f
},
{ // coefficient G
-3.351533e-06f, 0.03361262f, -0.09430492f, 0.1481656f, -0.09065784f, 0.03891745f
},
{ // coefficient H
2.021261f, 1.529938f, 2.211086f, 2.668578f, 0.2866983f, 1.602324f
},
{ // coefficient I
0.5742842f, 0.2851152f, 0.86322f, 0.6145923f, 0.6920973f, 0.6269169f
},
{ // radiance
0.00235195f, 0.002915308f, 0.003179213f, -0.008297787f, 0.1471589f, 0.08200387f
}
},
{ // turbidity 7
{ // coefficient A
-3.709341f, -3.393374f, -2.81282f, 0.254736f, -1.612173f, -1.098367f
},
{ // coefficient B
-2.038242f, -1.710497f, -2.678401f, 0.8840167f, -1.055541f, -0.5550631f
},
{ // coefficient C
-3.127214f, 1.661275f, -1.170428f, 0.5469168f, 0.4733463f, 0.4221569f
},
{ // coefficient D
3.28703f, -1.388896f, 1.560988f, -2.61553f, 5.083504f, -0.001661587f
},
{ // coefficient E
-0.04565671f, -0.1012293f, -0.4488218f, -1.048663f, -5.558403f, -4.970027f
},
{ // coefficient F
0.2427627f, 0.1676172f, 0.1088693f, 0.4823171f, 0.08029584f, 0.3479325f
},
{ // coefficient G
-7.50398e-06f, -0.0005719846f, 0.01391966f, -0.05958515f, 0.129418f, 0.04503648f
},
{ // coefficient H
1.817881f, 1.694199f, 2.144426f, 1.842479f, 0.7807964f, 1.369224f
},
{ // coefficient I
0.9000389f, -0.3140071f, 1.525731f, 0.5322138f, 0.3949822f, 0.6753984f
},
{ // radiance
0.002347797f, 0.002761449f, 0.003671854f, -0.01012039f, 0.1480091f, 0.08399335f
}
},
{ // turbidity 8
{ // coefficient A
-4.292777f, -4.016595f, -2.942646f, 0.9793627f, -2.085434f, -0.9660466f
},
{ // coefficient B
-2.2313f, -1.761175f, -3.350329f, 1.743715f, -1.445755f, -0.524741f
},
{ // coefficient C
-4.359252f, 2.616213f, -1.645817f, 0.5396545f, 0.3384806f, 0.4199048f
},
{ // coefficient D
4.562131f, -2.658657f, 2.957424f, -4.618273f, 7.702938f, -0.003304707f
},
{ // coefficient E
-0.03759725f, -0.08209207f, -0.1073783f, -2.186669f, -3.897179f, -5.014144f
},
{ // coefficient F
0.2170783f, 0.2736152f, -0.192005f, 1.012973f, -0.5947697f, 0.6733599f
},
{ // coefficient G
-6.945269e-06f, -0.000686417f, 0.01665992f, -0.07213786f, 0.1614194f, 0.02345161f
},
{ // coefficient H
1.79163f, 1.820262f, 1.723144f, 2.388103f, 0.7854754f, 1.225739f
},
{ // coefficient I
0.8996451f, -0.2712707f, 1.394962f, 0.6712519f, 0.3291034f, 0.7256834f
},
{ // radiance
0.00229625f, 0.002420416f, 0.005106803f, -0.0151546f, 0.1526421f, 0.08454641f
}
},
{ // turbidity 9
{ // coefficient A
-3.950771f, -6.38469f, -0.6207616f, -1.241493f, -0.439513f, -1.794977f
},
{ // coefficient B
-2.192673f, -2.515788f, -2.681061f, 0.590817f, -0.3700611f, -1.190586f
},
{ // coefficient C
-7.263653f, 3.874778f, -2.005212f, 0.3899777f, 0.2452754f, 0.3928989f
},
{ // coefficient D
7.343312f, -3.776079f, 2.445498f, -2.344038f, 3.602006f, 1.946953f
},
{ // coefficient E
-0.01840332f, -0.0348471f, -0.1935675f, -0.1820327f, -4.692896f, -1.874118f
},
{ // coefficient F
0.1893809f, 0.3311078f, -0.1988856f, 0.7889592f, -0.08620746f, -0.03292602f
},
{ // coefficient G
-3.583629e-06f, 0.008248395f, -0.01553754f, -0.01468064f, 0.1418548f, 0.02983309f
},
{ // coefficient H
1.901172f, 1.612712f, 2.21074f, 1.586758f, 1.521301f, 0.6737092f
},
{ // coefficient I
0.5902387f, 0.3123684f, 0.650704f, 1.007885f, 0.3989756f, 0.7322706f
},
{ // radiance
0.002193368f, 0.001826966f, 0.007606333f, -0.02312584f, 0.1588488f, 0.08547295f
}
},
{ // turbidity 10
{ // coefficient A
-8.896186f, -8.040403f, -0.415001f, -2.090923f, 0.5115239f, -2.334879f
},
{ // coefficient B
-3.27833f, -2.528514f, -3.539507f, 0.1673638f, 0.2415831f, -1.642495f
},
{ // coefficient C
-7.998349f, 3.774303f, -1.527733f, -0.2183495f, 0.4022318f, 0.2384458f
},
{ // coefficient D
8.023312f, -3.590457f, 1.415242f, -0.8374584f, 2.29295f, 3.068363f
},
{ // coefficient E
-0.01357555f, -0.03551313f, -0.1831476f, 0.02774184f, -4.061496f, -1.699588f
},
{ // coefficient F
0.194791f, 0.3650761f, -0.3009746f, 0.7912769f, 0.07453182f, -0.3936924f
},
{ // coefficient G
-5.390226e-06f, -0.0004401382f, 0.01211955f, -0.06650609f, 0.1902451f, 0.0302866f
},
{ // coefficient H
1.798414f, 1.701545f, 2.365976f, 1.154446f, 1.587339f, 0.4715374f
},
{ // coefficient I
0.7881395f, -0.006992899f, 0.992824f, 0.7863979f, 0.520762f, 0.7091893f
},
{ // radiance
0.002037533f, 0.0009415569f, 0.01104382f, -0.0334839f, 0.1637893f, 0.08792408f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-2.375941f, -2.605398f, -2.17626f, -0.7365744f, -1.017537f, -1.025215f
},
{ // coefficient B
-1.508643f, -1.58916f, -1.601522f, -0.3720296f, -0.4631811f, -0.3712574f
},
{ // coefficient C
-5.070151f, -1.13756f, 0.7060324f, 0.7315328f, 1.093174f, 0.9661278f
},
{ // coefficient D
5.509378f, 1.827729f, -1.060717f, 0.3156067f, -0.07215941f, 0.007791398f
},
{ // coefficient E
-0.02915769f, -0.04939637f, -0.05144614f, -0.2739385f, 0.5408457f, -4.508617f
},
{ // coefficient F
0.2122471f, 0.3243952f, -0.1938031f, 0.8252942f, -0.01848322f, 0.4371074f
},
{ // coefficient G
8.584007e-05f, -0.0006464612f, 0.003449257f, -0.01055013f, 0.0194299f, -1.241898e-06f
},
{ // coefficient H
1.517285f, 1.008816f, 2.641594f, 0.0732761f, 0.6882763f, 1.152655f
},
{ // coefficient I
0.5163253f, 0.4568463f, 0.5976852f, 0.3218251f, 0.7268524f, 0.500824f
},
{ // radiance
0.003061048f, 0.002126839f, 0.01132767f, -0.02788848f, 0.194861f, 0.1618476f
}
},
{ // turbidity 2
{ // coefficient A
-2.621793f, -2.47333f, -2.843519f, -0.1805184f, -1.274753f, -0.9224434f
},
{ // coefficient B
-1.611544f, -1.495083f, -1.913751f, -0.03670063f, -0.5831501f, -0.3344822f
},
{ // coefficient C
-4.49473f, -0.5536781f, 0.2006296f, 0.7583717f, 0.9711756f, 0.9964647f
},
{ // coefficient D
4.902741f, 1.305457f, -0.8580922f, 0.7594739f, -0.6209175f, 0.3991735f
},
{ // coefficient E
-0.02935751f, -0.07261347f, -0.05273753f, -0.2092951f, 0.1488633f, -4.951439f
},
{ // coefficient F
0.1930048f, 0.2967738f, -0.0780025f, 0.6278666f, 0.2117108f, 0.4332347f
},
{ // coefficient G
1.821579e-05f, -0.0002858526f, 0.002395781f, -0.008086828f, 0.01531499f, -1.082832e-05f
},
{ // coefficient H
1.522018f, 1.13265f, 2.505017f, 0.4136002f, 0.9306008f, 0.9570977f
},
{ // coefficient I
0.6563013f, 0.1737068f, 0.9023934f, 0.6026937f, 0.7201852f, 0.5221054f
},
{ // radiance
0.00304718f, 0.002153513f, 0.01116935f, -0.02810228f, 0.1949994f, 0.1614103f
}
},
{ // turbidity 3
{ // coefficient A
-2.6481f, -3.004556f, -2.337937f, -0.5985994f, -1.155357f, -1.010658f
},
{ // coefficient B
-1.58772f, -1.702983f, -1.584523f, -0.2846782f, -0.4638306f, -0.3816787f
},
{ // coefficient C
-3.738681f, 0.1060112f, -0.2912486f, 0.8903642f, 0.8041892f, 0.9644918f
},
{ // coefficient D
4.084015f, 0.5816666f, -0.4467895f, 0.5578845f, -0.4051351f, 0.2144477f
},
{ // coefficient E
-0.03301163f, -0.08140777f, -0.09195281f, -0.2510042f, -0.2819435f, -5.005667f
},
{ // coefficient F
0.173614f, 0.3584557f, -0.2355753f, 0.8033778f, 0.08505708f, 0.5475125f
},
{ // coefficient G
-5.598265e-06f, -7.793874e-05f, 0.001887118f, -0.00992046f, 0.02749222f, 3.758162e-05f
},
{ // coefficient H
1.628176f, 1.090228f, 3.053238f, 0.07388584f, 1.33482f, 1.033427f
},
{ // coefficient I
0.8967917f, -0.1079451f, 1.043382f, 0.7097127f, 0.521662f, 0.6866266f
},
{ // radiance
0.00303892f, 0.002051613f, 0.01130934f, -0.02861029f, 0.1941964f, 0.1617911f
}
},
{ // turbidity 4
{ // coefficient A
-2.686681f, -2.684912f, -2.587652f, -0.2679506f, -1.26716f, -0.9570978f
},
{ // coefficient B
-1.694114f, -1.616888f, -1.999748f, 0.0513974f, -0.7441021f, -0.3478847f
},
{ // coefficient C
-4.346908f, 2.447798f, -1.32581f, 1.276587f, 0.9154665f, 0.9709923f
},
{ // coefficient D
4.745876f, -1.626883f, 0.7561102f, -0.2286956f, 0.8190436f, -0.003444499f
},
{ // coefficient E
-0.02879846f, -0.07346183f, -0.4182131f, 0.7388924f, -6.805266f, -4.677921f
},
{ // coefficient F
0.2237339f, 0.2641722f, -0.04285017f, 0.5606395f, 0.1013064f, 0.3551546f
},
{ // coefficient G
-6.438656e-06f, -0.0001001518f, 0.002719759f, -0.01472508f, 0.04140297f, 0.006241534f
},
{ // coefficient H
1.456353f, 1.045501f, 2.420598f, 0.5105614f, 0.3378301f, 1.05206f
},
{ // coefficient I
0.900034f, -0.2314856f, 1.281981f, 0.7670396f, 0.312886f, 0.7543152f
},
{ // radiance
0.002963024f, 0.00186915f, 0.01189063f, -0.03088695f, 0.1962576f, 0.1593205f
}
},
{ // turbidity 5
{ // coefficient A
-3.09383f, -3.121106f, -2.166025f, -0.5263078f, -1.138361f, -1.013651f
},
{ // coefficient B
-1.859478f, -1.664385f, -2.172663f, 0.2296403f, -0.8186096f, -0.3811416f
},
{ // coefficient C
-4.003711f, 2.868256f, -1.705862f, 1.342025f, 0.88457f, 0.9420266f
},
{ // coefficient D
4.391665f, -2.280292f, 1.960864f, -2.03459f, 3.291911f, -0.003599378f
},
{ // coefficient E
-0.03051581f, -0.08456847f, -0.218502f, -1.024013f, -5.915866f, -4.895655f
},
{ // coefficient F
0.2180265f, 0.2227469f, 0.01891067f, 0.4873264f, 0.1125846f, 0.2864445f
},
{ // coefficient G
-7.401285e-06f, -0.0001681252f, 0.004423413f, -0.01982506f, 0.04477952f, 0.01940262f
},
{ // coefficient H
1.448366f, 1.176332f, 2.071931f, 0.9818474f, 0.09195132f, 1.026648f
},
{ // coefficient I
0.9000549f, -0.2605895f, 1.297661f, 1.040041f, -0.01138939f, 0.7421204f
},
{ // radiance
0.002918936f, 0.001726267f, 0.01250792f, -0.03337994f, 0.1973927f, 0.1577752f
}
},
{ // turbidity 6
{ // coefficient A
-2.379373f, -3.200327f, -1.107956f, -0.9423359f, -1.037343f, -1.002208f
},
{ // coefficient B
-1.540348f, -2.016985f, -1.330969f, 0.04619106f, -0.571108f, -0.3271512f
},
{ // coefficient C
-1.318467f, 0.9935187f, -0.1627216f, 0.03116348f, 1.282899f, 0.7288675f
},
{ // coefficient D
1.724703f, -0.9970906f, 2.985435f, -6.353122f, 8.808032f, 0.795056f
},
{ // coefficient E
-0.1017147f, -0.2159523f, -0.02198633f, -6.192769f, -0.8741903f, -5.029931f
},
{ // coefficient F
0.1811106f, 0.3183812f, -0.4401148f, 1.175886f, -0.6097972f, 0.4493639f
},
{ // coefficient G
-3.047951e-06f, 0.01575987f, -0.05433064f, 0.1020452f, -0.05996612f, 0.02389923f
},
{ // coefficient H
1.598808f, 1.180131f, 1.701087f, 2.180417f, 0.1533313f, 1.568465f
},
{ // coefficient I
0.579658f, 0.283951f, 0.8289746f, 0.7076018f, 0.5842877f, 0.565391f
},
{ // radiance
0.002855489f, 0.001647214f, 0.01256484f, -0.03387525f, 0.1965371f, 0.1571482f
}
},
{ // turbidity 7
{ // coefficient A
-3.584221f, -3.579609f, -2.053811f, -0.2602659f, -1.266632f, -1.021321f
},
{ // coefficient B
-2.140498f, -1.80442f, -2.42085f, 0.596332f, -0.8504227f, -0.5129049f
},
{ // coefficient C
-5.324613f, 3.44849f, -1.795999f, 1.059632f, 0.8680693f, 0.9006232f
},
{ // coefficient D
5.753381f, -2.929179f, 1.835004f, -1.214076f, 1.899473f, 1.534018f
},
{ // coefficient E
-0.02662581f, -0.06009529f, -0.34518f, 0.6424455f, -8.680238f, -3.672948f
},
{ // coefficient F
0.2099272f, 0.2821561f, -0.1042135f, 0.6890202f, -0.1798563f, 0.2301115f
},
{ // coefficient G
-6.879519e-06f, -0.0006203809f, 0.01511574f, -0.06544341f, 0.1477665f, 0.005151145f
},
{ // coefficient H
1.362229f, 1.312035f, 1.751304f, 1.457883f, 0.2430031f, 0.7460819f
},
{ // coefficient I
0.9000467f, -0.3013934f, 1.491704f, 0.5321707f, 0.4229871f, 0.5418252f
},
{ // radiance
0.002825443f, 0.001406555f, 0.01336433f, -0.03675617f, 0.1982073f, 0.153578f
}
},
{ // turbidity 8
{ // coefficient A
-4.095186f, -3.691554f, -2.697067f, 0.7049827f, -1.771411f, -0.9623648f
},
{ // coefficient B
-2.274183f, -1.654418f, -3.473351f, 1.76987f, -1.35827f, -0.551847f
},
{ // coefficient C
-4.11516f, 3.158573f, -1.707297f, 0.8659868f, 0.7604506f, 0.9623684f
},
{ // coefficient D
4.514741f, -2.961366f, 3.138823f, -4.445404f, 7.19159f, -0.005697517f
},
{ // coefficient E
-0.0313144f, -0.07834161f, -0.0832343f, -2.256134f, -4.358366f, -5.015884f
},
{ // coefficient F
0.1475658f, 0.3035141f, -0.1605972f, 0.7768538f, -0.3375251f, 0.2811964f
},
{ // coefficient G
-5.329133e-06f, -0.0004241191f, 0.01025052f, -0.04741793f, 0.1136852f, 0.02100168f
},
{ // coefficient H
1.411833f, 1.4852f, 1.132282f, 2.212406f, 0.1844576f, 0.4434131f
},
{ // coefficient I
0.8893477f, -0.1689443f, 1.093318f, 1.109562f, -0.001208189f, 0.7212648f
},
{ // radiance
0.002678428f, 0.001109106f, 0.01431453f, -0.04011878f, 0.1979278f, 0.1510186f
}
},
{ // turbidity 9
{ // coefficient A
-5.131552f, -6.19291f, -0.6741018f, -0.8312435f, -0.7455272f, -1.448374f
},
{ // coefficient B
-2.558716f, -2.261479f, -3.356237f, 1.325391f, -0.8504579f, -0.9739004f
},
{ // coefficient C
-8.066753f, 5.059205f, -2.207329f, 0.5324159f, 0.9021474f, 0.8801628f
},
{ // coefficient D
8.403192f, -4.892079f, 3.300094f, -2.912438f, 3.426594f, 1.705262f
},
{ // coefficient E
-0.01450692f, -0.02703716f, -0.180112f, -0.08384977f, -4.607412f, -2.420298f
},
{ // coefficient F
0.1494495f, 0.3381259f, -0.2560242f, 0.7600872f, -0.1706127f, -0.01077392f
},
{ // coefficient G
-2.898774e-06f, 0.02231257f, -0.06216731f, 0.07749519f, 0.04792024f, 0.02691713f
},
{ // coefficient H
1.427512f, 1.490697f, 1.180699f, 2.129188f, 0.2354077f, 0.1854876f
},
{ // coefficient I
0.5020826f, 0.5441761f, 0.2241782f, 1.328287f, 0.2718195f, 0.7097466f
},
{ // radiance
0.00255575f, 0.0006244779f, 0.01562522f, -0.04466841f, 0.1968517f, 0.1462399f
}
},
{ // turbidity 10
{ // coefficient A
-8.672396f, -7.852799f, -0.1374103f, -2.211224f, 0.5932825f, -2.098394f
},
{ // coefficient B
-3.339107f, -2.60612f, -3.570476f, 0.1190704f, 0.2482606f, -1.561705f
},
{ // coefficient C
-7.666257f, 4.23184f, -1.588768f, 0.06700025f, 0.9239617f, 0.7602298f
},
{ // coefficient D
7.916291f, -3.774406f, 1.538806f, -0.8972445f, 2.382787f, 1.783879f
},
{ // coefficient E
-0.01396976f, -0.03540722f, -0.1923629f, 0.2133056f, -4.611846f, -1.486238f
},
{ // coefficient F
0.1676538f, 0.2178494f, -0.01793545f, 0.447173f, 0.1608983f, -0.4025919f
},
{ // coefficient G
-5.816416e-06f, 6.198617e-05f, 0.00337395f, -0.0314815f, 0.138559f, 0.02965074f
},
{ // coefficient H
1.448247f, 1.295236f, 2.008581f, 0.9759721f, 0.7641781f, -0.0871718f
},
{ // coefficient I
0.8178892f, 0.1130678f, 0.6041668f, 1.166537f, 0.3076032f, 0.7279518f
},
{ // radiance
0.00232392f, 0.0001300657f, 0.01663828f, -0.04826021f, 0.1889056f, 0.142731f
}
}
}
},
{ // 400.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.8696f, -1.746138f, -1.536453f, -1.06978f, -1.060384f, -1.077246f
},
{ // coefficient B
-0.9575785f, -1.001117f, -0.5330479f, -0.5815078f, -0.2207518f, -0.299304f
},
{ // coefficient C
-6.993871f, -1.758295f, 1.12683f, 0.8612956f, 0.6362691f, 0.816743f
},
{ // coefficient D
7.306207f, 2.492931f, -2.452367f, 1.301347f, -0.5757237f, 0.2203982f
},
{ // coefficient E
-0.0324032f, -0.06202861f, -0.06694842f, -0.4089489f, 0.4275177f, -3.984213f
},
{ // coefficient F
0.3834949f, 0.4442043f, -0.02983825f, 1.269432f, 0.08443714f, 1.123699f
},
{ // coefficient G
4.369976e-05f, -0.000377955f, 0.002320196f, -0.009018004f, 0.02082596f, 0.01313138f
},
{ // coefficient H
2.254773f, 1.411622f, 5.488905f, -2.283572f, 3.12504f, 1.750491f
},
{ // coefficient I
0.5025988f, 0.4890101f, 0.5403856f, 0.3760374f, 0.7891611f, 0.4999653f
},
{ // radiance
0.005767645f, 0.01219302f, -0.02188467f, 0.08262275f, 0.1719839f, 0.1233791f
}
},
{ // turbidity 2
{ // coefficient A
-1.752499f, -1.888565f, -1.471013f, -1.061905f, -1.0712f, -1.081645f
},
{ // coefficient B
-0.9053927f, -1.067725f, -0.5497976f, -0.5429222f, -0.2263246f, -0.3128264f
},
{ // coefficient C
-10.80943f, 1.491851f, -0.4310423f, 1.412232f, 0.3639469f, 0.8436471f
},
{ // coefficient D
11.10516f, -0.7934014f, -0.8787275f, 0.8460977f, -0.6109244f, 0.3884148f
},
{ // coefficient E
-0.0209209f, -0.04853718f, -0.09206447f, -0.2973654f, -0.2484665f, -5.004662f
},
{ // coefficient F
0.3348109f, 0.5346693f, -0.1490862f, 1.357299f, -0.07097027f, 1.137f
},
{ // coefficient G
-8.432141e-06f, 9.17037e-05f, -4.078054e-05f, -0.005347332f, 0.04735641f, 0.01500838f
},
{ // coefficient H
2.326824f, 1.397595f, 5.442983f, -2.444304f, 3.445733f, 1.585647f
},
{ // coefficient I
0.90005f, -0.363573f, 1.75701f, -0.03363016f, 0.8486491f, 0.5832608f
},
{ // radiance
0.005661981f, 0.01258489f, -0.02324339f, 0.08372421f, 0.1730981f, 0.1255797f
}
},
{ // turbidity 3
{ // coefficient A
-1.774187f, -1.849342f, -1.624679f, -0.8692395f, -1.169258f, -1.058391f
},
{ // coefficient B
-0.9327599f, -1.038735f, -0.725706f, -0.3025565f, -0.3711802f, -0.2657785f
},
{ // coefficient C
-9.130103f, 2.863013f, -1.493033f, 1.630221f, 0.3440155f, 0.6984363f
},
{ // coefficient D
9.413272f, -2.260302f, 0.5658415f, -0.04624355f, -0.1044556f, 0.09737578f
},
{ // coefficient E
-0.02448089f, -0.05040252f, -0.2079847f, 0.08408669f, -3.304858f, -1.903943f
},
{ // coefficient F
0.3487996f, 0.4583723f, -0.0465118f, 1.091981f, 0.1185883f, 0.8963329f
},
{ // coefficient G
-3.033497e-06f, 0.0100869f, -0.03701743f, 0.06506253f, 0.01489352f, 0.0648086f
},
{ // coefficient H
2.291071f, 1.502617f, 4.858534f, -1.403006f, 2.642598f, 2.034024f
},
{ // coefficient I
0.640836f, 0.07223351f, 1.342775f, 0.2076066f, 0.8151882f, 0.5870424f
},
{ // radiance
0.005644031f, 0.01248986f, -0.02287316f, 0.07999388f, 0.1815345f, 0.1252983f
}
},
{ // turbidity 4
{ // coefficient A
-1.898858f, -2.136007f, -1.355343f, -0.9063816f, -1.17047f, -1.085813f
},
{ // coefficient B
-1.031814f, -1.206456f, -0.6736369f, -0.1873797f, -0.3968082f, -0.3519543f
},
{ // coefficient C
-9.726056f, 3.801172f, -2.05262f, 1.559544f, 0.2266604f, 0.7537557f
},
{ // coefficient D
10.00989f, -3.254852f, 1.298239f, -0.2631298f, -0.2018118f, 0.3003522f
},
{ // coefficient E
-0.02125268f, -0.0451649f, -0.1986869f, 0.05988684f, -3.768512f, -1.811803f
},
{ // coefficient F
0.3569897f, 0.4001123f, 0.06321488f, 0.9375378f, 0.09706445f, 0.7945754f
},
{ // coefficient G
-2.706653e-06f, 0.0215189f, -0.06792635f, 0.09681349f, 0.0604666f, 0.05293207f
},
{ // coefficient H
2.147646f, 1.481457f, 4.482593f, -0.648089f, 2.7232f, 1.335214f
},
{ // coefficient I
0.5982401f, 0.1444904f, 1.322565f, 0.2362949f, 0.7968878f, 0.638598f
},
{ // radiance
0.005479152f, 0.01215496f, -0.02122586f, 0.07189527f, 0.1962517f, 0.1245648f
}
},
{ // turbidity 5
{ // coefficient A
-1.74324f, -2.624196f, -1.040612f, -0.9537244f, -1.163791f, -1.118058f
},
{ // coefficient B
-0.9391727f, -1.516703f, -0.6103044f, -0.0068958f, -0.5095388f, -0.3706501f
},
{ // coefficient C
-8.905876f, 5.053093f, -3.190021f, 1.788307f, 0.1215079f, 0.688518f
},
{ // coefficient D
9.139326f, -4.599198f, 3.216574f, -2.659655f, 3.427998f, -0.001596776f
},
{ // coefficient E
-0.02333803f, -0.0424873f, -0.3285978f, 0.6473584f, -9.243257f, -3.279613f
},
{ // coefficient F
0.3093783f, 0.482025f, -0.1982489f, 1.333544f, -0.5059496f, 1.068229f
},
{ // coefficient G
-2.051734e-06f, 0.01829762f, -0.05367025f, 0.05529579f, 0.1412681f, 0.03010797f
},
{ // coefficient H
2.320564f, 1.216991f, 3.984414f, 0.8049825f, 1.829389f, 1.479756f
},
{ // coefficient I
0.5474845f, 0.2739806f, 1.158312f, 0.4930209f, 0.554461f, 0.7483765f
},
{ // radiance
0.005385972f, 0.01187505f, -0.02025911f, 0.0660251f, 0.2067312f, 0.126491f
}
},
{ // turbidity 6
{ // coefficient A
-1.811701f, -2.74618f, -0.955877f, -0.9065351f, -1.180081f, -1.128755f
},
{ // coefficient B
-1.00265f, -1.592055f, -0.640179f, 0.1443765f, -0.5441674f, -0.3892532f
},
{ // coefficient C
-9.194183f, 4.225172f, -2.117905f, 0.7871576f, 0.3963103f, 0.5327884f
},
{ // coefficient D
9.469966f, -3.929912f, 2.293799f, -1.631026f, 1.375519f, 1.825253f
},
{ // coefficient E
-0.02224886f, -0.03866297f, -0.2226591f, 0.1537369f, -4.882926f, -2.335719f
},
{ // coefficient F
0.3237441f, 0.398244f, 0.06151477f, 0.8353048f, -0.0009391794f, 0.5863162f
},
{ // coefficient G
-2.632967e-06f, 0.03812089f, -0.1223358f, 0.1849704f, 0.06455569f, 0.05535626f
},
{ // coefficient H
2.168879f, 1.390284f, 3.475975f, 1.650615f, 1.77467f, 1.452293f
},
{ // coefficient I
0.5952956f, 0.1922369f, 1.124575f, 0.4473559f, 0.6989373f, 0.692009f
},
{ // radiance
0.00531997f, 0.01160513f, -0.01904232f, 0.06030292f, 0.2158311f, 0.1261204f
}
},
{ // turbidity 7
{ // coefficient A
-2.042064f, -2.700541f, -1.187417f, -0.5639857f, -1.363103f, -1.102449f
},
{ // coefficient B
-1.144943f, -1.485611f, -1.10263f, 0.717077f, -0.8396317f, -0.3685043f
},
{ // coefficient C
-5.152625f, 2.159154f, -0.940469f, -0.1146502f, 0.5750351f, 0.4622549f
},
{ // coefficient D
5.384535f, -2.188608f, 2.427532f, -3.487513f, 4.317766f, -0.002018634f
},
{ // coefficient E
-0.03519943f, -0.05879735f, -0.2948903f, -0.7711076f, -2.364722f, -3.947538f
},
{ // coefficient F
0.3025845f, 0.3921779f, -0.03437257f, 1.046031f, -0.4135433f, 0.8449276f
},
{ // coefficient G
-2.994849e-06f, 0.05020849f, -0.1636446f, 0.2606854f, 0.02669162f, 0.08837036f
},
{ // coefficient H
2.113418f, 1.869923f, 1.894926f, 3.87352f, 0.57687f, 1.601304f
},
{ // coefficient I
0.6000255f, 0.2118398f, 1.001327f, 0.563798f, 0.6649078f, 0.702698f
},
{ // radiance
0.005179289f, 0.01125842f, -0.01708641f, 0.05111365f, 0.228503f, 0.127826f
}
},
{ // turbidity 8
{ // coefficient A
-2.314053f, -2.630411f, -1.884599f, 0.06351036f, -1.672505f, -1.007695f
},
{ // coefficient B
-1.314766f, -1.404258f, -1.657069f, 1.061776f, -0.975271f, -0.3840375f
},
{ // coefficient C
-7.125646f, 3.675058f, -1.731916f, 0.01195544f, 0.528286f, 0.3463543f
},
{ // coefficient D
7.357344f, -3.804226f, 3.034605f, -3.018584f, 3.776331f, 2.848366f
},
{ // coefficient E
-0.0258571f, -0.04249037f, -0.1746846f, -0.5618808f, -4.325128f, -2.105699f
},
{ // coefficient F
0.2863908f, 0.3486414f, -0.01559184f, 1.008146f, -0.4384279f, 0.4636429f
},
{ // coefficient G
-2.425744e-06f, 0.05675141f, -0.1600465f, 0.2013783f, 0.1352387f, 0.02800131f
},
{ // coefficient H
2.039404f, 1.944935f, 1.915108f, 3.212273f, 0.8847662f, 1.306056f
},
{ // coefficient I
0.5594003f, 0.2880711f, 0.9460232f, 0.6523624f, 0.5717158f, 0.7618236f
},
{ // radiance
0.004994685f, 0.01025385f, -0.01254777f, 0.03429945f, 0.249031f, 0.1302691f
}
},
{ // turbidity 9
{ // coefficient A
-2.865406f, -3.196334f, -2.335492f, 0.3278911f, -1.738015f, -1.006457f
},
{ // coefficient B
-1.637174f, -1.512711f, -2.353032f, 1.315177f, -1.091057f, -0.4243112f
},
{ // coefficient C
-7.429613f, 3.94788f, -1.764665f, -0.1438189f, 0.5425696f, 0.1934952f
},
{ // coefficient D
7.592861f, -4.029796f, 2.841986f, -2.85963f, 4.650166f, 2.79503f
},
{ // coefficient E
-0.02196505f, -0.0369962f, -0.07331326f, -0.8436262f, -3.97519f, -1.680697f
},
{ // coefficient F
0.2800873f, 0.1461452f, 0.4537047f, 0.2645711f, 0.2066035f, -0.09428375f
},
{ // coefficient G
-3.90853e-06f, 0.05921432f, -0.1668f, 0.2154409f, 0.1017332f, 0.07158314f
},
{ // coefficient H
1.962505f, 1.806132f, 2.156269f, 2.264493f, 0.9321625f, 1.082383f
},
{ // coefficient I
0.5891464f, 0.2331816f, 0.9812486f, 0.5975203f, 0.6517783f, 0.7234775f
},
{ // radiance
0.004759538f, 0.008205142f, -0.0032463f, 0.003909938f, 0.2819549f, 0.132243f
}
},
{ // turbidity 10
{ // coefficient A
-4.195701f, -4.119053f, -4.016823f, 1.066227f, -1.855594f, -1.014961f
},
{ // coefficient B
-2.171135f, -1.648023f, -3.599323f, 1.392783f, -1.0293f, -0.6014045f
},
{ // coefficient C
-5.278016f, 2.332552f, -0.6179057f, -0.9653695f, 0.8521013f, 0.01926619f
},
{ // coefficient D
5.393085f, -2.173813f, 0.6454998f, -0.5471637f, 3.975027f, 2.357335f
},
{ // coefficient E
-0.02419182f, -0.06442977f, -0.09913704f, -0.8500569f, -3.806819f, -1.312911f
},
{ // coefficient F
0.2050075f, 0.2188945f, 0.2770986f, 0.4678476f, -0.02044703f, -0.08783609f
},
{ // coefficient G
-3.797795e-06f, 0.03548565f, -0.09265471f, 0.08760324f, 0.1757298f, 0.07739369f
},
{ // coefficient H
1.853402f, 1.792598f, 2.227501f, 1.564314f, 0.7790123f, 0.735677f
},
{ // coefficient I
0.6912927f, 0.04162082f, 1.179216f, 0.4885251f, 0.6918921f, 0.7042591f
},
{ // radiance
0.004412341f, 0.00491021f, 0.01040886f, -0.03627125f, 0.3147182f, 0.1409331f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.467874f, -1.863242f, -1.384101f, -0.9828878f, -1.057001f, -1.058165f
},
{ // coefficient B
-0.7636581f, -1.048209f, -0.5553588f, -0.4220161f, -0.2727017f, -0.243613f
},
{ // coefficient C
-7.251111f, -1.580541f, 1.191974f, 1.184916f, 1.000018f, 1.074414f
},
{ // coefficient D
7.726558f, 2.55803f, -2.157512f, 0.9230716f, -0.3108668f, 0.08554434f
},
{ // coefficient E
-0.02759341f, -0.05622126f, -0.03567589f, -0.4062756f, 0.6848382f, -3.846431f
},
{ // coefficient F
0.3527534f, 0.305396f, 0.1020018f, 1.06498f, -0.1682596f, 0.7595465f
},
{ // coefficient G
5.340629e-05f, -0.0004657733f, 0.002940191f, -0.01060088f, 0.02301218f, 9.729528e-06f
},
{ // coefficient H
2.007747f, 0.995639f, 4.555372f, -1.764815f, 2.075821f, 1.915942f
},
{ // coefficient I
0.5447487f, 0.3409087f, 0.8736043f, 0.2932808f, 0.7647327f, 0.4999612f
},
{ // radiance
0.007016633f, 0.009796846f, -0.0009823849f, 0.02324224f, 0.303501f, 0.257385f
}
},
{ // turbidity 2
{ // coefficient A
-1.610179f, -2.004083f, -1.178554f, -1.091191f, -0.9987232f, -1.075477f
},
{ // coefficient B
-0.8336019f, -1.148388f, -0.5064839f, -0.4200919f, -0.2073656f, -0.2556973f
},
{ // coefficient C
-14.43356f, 3.747156f, -1.117126f, 1.826508f, 0.7443384f, 1.05169f
},
{ // coefficient D
14.92556f, -2.80856f, 0.281574f, 0.07131992f, -0.07245157f, 0.0307596f
},
{ // coefficient E
-0.01381931f, -0.03298677f, -0.07068438f, -0.1883417f, -0.5588818f, -1.895492f
},
{ // coefficient F
0.2902743f, 0.4224754f, -0.03031589f, 1.061437f, -0.1364188f, 0.6408333f
},
{ // coefficient G
-7.665717e-06f, 0.0004166438f, -0.003468069f, 0.01311913f, 0.02023795f, 0.02109454f
},
{ // coefficient H
1.981513f, 0.879211f, 4.539093f, -1.97255f, 2.829822f, 1.89124f
},
{ // coefficient I
0.9000423f, -0.2321793f, 1.385612f, 0.2511246f, 0.7230021f, 0.5652527f
},
{ // radiance
0.006984411f, 0.009584282f, -0.0005945671f, 0.02061312f, 0.3090019f, 0.2547364f
}
},
{ // turbidity 3
{ // coefficient A
-1.531711f, -2.113444f, -1.19672f, -1.003096f, -1.061048f, -1.054623f
},
{ // coefficient B
-0.8266123f, -1.209106f, -0.5485073f, -0.3200944f, -0.2908451f, -0.2267971f
},
{ // coefficient C
-9.324763f, 3.526508f, -1.667136f, 2.042427f, 0.6390633f, 1.028491f
},
{ // coefficient D
9.811491f, -2.62873f, 0.9920197f, -0.4417822f, 0.1406786f, -0.003197518f
},
{ // coefficient E
-0.0198856f, -0.04634065f, -0.1622954f, -0.1404599f, -1.834706f, -1.705329f
},
{ // coefficient F
0.3404598f, 0.3182739f, 0.06951315f, 0.9288535f, -0.1432145f, 0.6747358f
},
{ // coefficient G
-2.068369e-06f, 0.009500212f, -0.03482877f, 0.06077264f, 0.0191408f, 0.02072713f
},
{ // coefficient H
1.869307f, 1.004871f, 4.214632f, -1.386737f, 2.284516f, 2.140509f
},
{ // coefficient I
0.5860842f, 0.2109968f, 1.108967f, 0.4557129f, 0.5965839f, 0.7205824f
},
{ // radiance
0.006865141f, 0.009540856f, -0.0003311701f, 0.01726495f, 0.3147099f, 0.2539552f
}
},
{ // turbidity 4
{ // coefficient A
-1.610001f, -2.308577f, -0.9441254f, -1.214397f, -0.9377631f, -1.101428f
},
{ // coefficient B
-0.8622601f, -1.440194f, -0.3064812f, -0.5423074f, -0.1533211f, -0.3069552f
},
{ // coefficient C
-10.60181f, 2.937335f, -0.8895068f, 1.361001f, 0.8439993f, 0.9687591f
},
{ // coefficient D
11.06133f, -2.093599f, 0.2815271f, 0.1782273f, -0.3893771f, 0.4500576f
},
{ // coefficient E
-0.01648296f, -0.03430217f, -0.08036228f, -0.3648675f, -0.2564779f, -4.912444f
},
{ // coefficient F
0.2590559f, 0.3427774f, 0.1867017f, 0.7181838f, -0.06742272f, 0.5398387f
},
{ // coefficient G
-1.904522e-06f, 0.02590705f, -0.07809247f, 0.1135547f, 0.02255856f, 0.03638172f
},
{ // coefficient H
1.930415f, 0.567375f, 4.774631f, -1.935982f, 2.780601f, 1.582999f
},
{ // coefficient I
0.6006964f, 0.1250422f, 1.391904f, 0.1820361f, 0.7972008f, 0.642595f
},
{ // radiance
0.006701191f, 0.009352606f, 0.0003392916f, 0.01159886f, 0.3228446f, 0.2530111f
}
},
{ // turbidity 5
{ // coefficient A
-1.687541f, -2.278432f, -1.302702f, -0.7886965f, -1.15776f, -1.04725f
},
{ // coefficient B
-0.937817f, -1.387936f, -0.7200083f, -0.03966453f, -0.4084596f, -0.2873072f
},
{ // coefficient C
-8.475686f, 5.425292f, -3.084331f, 2.110076f, 0.5689088f, 1.046858f
},
{ // coefficient D
8.929279f, -4.725025f, 3.191487f, -2.232935f, 2.37959f, 0.6890046f
},
{ // coefficient E
-0.02132746f, -0.03892912f, -0.3222828f, 0.7269719f, -9.687743f, -2.947892f
},
{ // coefficient F
0.291765f, 0.4084727f, -0.1493849f, 1.16341f, -0.5598487f, 0.7245997f
},
{ // coefficient G
-2.177239e-06f, 0.01842665f, -0.05641672f, 0.07133744f, 0.09191895f, 0.01125227f
},
{ // coefficient H
1.863407f, 0.8461986f, 3.624218f, -0.1660119f, 1.923047f, 1.352625f
},
{ // coefficient I
0.5908179f, 0.1673366f, 1.256086f, 0.4329084f, 0.5802927f, 0.7193979f
},
{ // radiance
0.006513679f, 0.008873317f, 0.00233952f, 0.002817964f, 0.3341256f, 0.2496316f
}
},
{ // turbidity 6
{ // coefficient A
-1.643764f, -2.473069f, -1.232599f, -0.6095289f, -1.324655f, -0.9888131f
},
{ // coefficient B
-0.9086683f, -1.505194f, -0.7896777f, 0.2745468f, -0.6255071f, -0.2584852f
},
{ // coefficient C
-8.750078f, 5.448737f, -3.056911f, 1.809782f, 0.6240868f, 1.05216f
},
{ // coefficient D
9.165653f, -4.863726f, 3.546334f, -2.593032f, 2.021182f, 0.7264224f
},
{ // coefficient E
-0.02025728f, -0.03662439f, -0.194882f, -0.01710009f, -5.87605f, -2.563426f
},
{ // coefficient F
0.2737848f, 0.1938729f, 0.3344216f, 0.5537278f, -0.1507819f, 0.5494058f
},
{ // coefficient G
-2.240987e-06f, 0.04741355f, -0.1376967f, 0.1935506f, 0.0330125f, 0.01821983f
},
{ // coefficient H
1.943471f, 0.8586572f, 3.029469f, 1.259882f, 1.142019f, 1.277896f
},
{ // coefficient I
0.5731722f, 0.2284408f, 1.120573f, 0.4970103f, 0.5986671f, 0.7557967f
},
{ // radiance
0.006405833f, 0.008466409f, 0.003661909f, -0.003045984f, 0.3414328f, 0.2464327f
}
},
{ // turbidity 7
{ // coefficient A
-2.100689f, -2.61061f, -0.8340903f, -0.9180308f, -1.126819f, -1.09161f
},
{ // coefficient B
-1.204431f, -1.587799f, -0.7487965f, 0.3752197f, -0.665233f, -0.3433417f
},
{ // coefficient C
-7.573121f, 5.460928f, -3.154774f, 1.471125f, 0.7386065f, 1.042206f
},
{ // coefficient D
8.039632f, -5.158708f, 4.600448f, -4.691789f, 5.362044f, 0.1075776f
},
{ // coefficient E
-0.02300526f, -0.0450565f, -0.1818975f, -0.5128294f, -6.196453f, -4.44394f
},
{ // coefficient F
0.2839214f, 0.3876929f, -0.1871764f, 1.164707f, -0.7280092f, 0.7469895f
},
{ // coefficient G
-2.346958e-06f, 0.03852459f, -0.1222094f, 0.1831224f, 0.06312604f, 0.03166247f
},
{ // coefficient H
1.678847f, 1.172854f, 2.067197f, 2.900937f, 0.2874666f, 1.019495f
},
{ // coefficient I
0.5536303f, 0.3104745f, 0.8802127f, 0.7718397f, 0.422558f, 0.7523771f
},
{ // radiance
0.006220899f, 0.007997469f, 0.005957591f, -0.01302863f, 0.3523355f, 0.2418785f
}
},
{ // turbidity 8
{ // coefficient A
-2.234623f, -2.566352f, -1.577477f, -0.2747774f, -1.433045f, -0.9961738f
},
{ // coefficient B
-1.310221f, -1.472664f, -1.491725f, 0.878706f, -0.8939972f, -0.302396f
},
{ // coefficient C
-8.525764f, 5.894182f, -3.06584f, 1.071723f, 0.853809f, 0.9152994f
},
{ // coefficient D
8.932373f, -5.619094f, 4.446634f, -4.16923f, 4.481475f, 2.936173f
},
{ // coefficient E
-0.01814945f, -0.03204346f, -0.1689128f, -0.3029966f, -5.978961f, -2.31106f
},
{ // coefficient F
0.2615153f, 0.2993298f, 0.09559287f, 0.6326611f, -0.1608369f, 0.1216642f
},
{ // coefficient G
-2.992344e-06f, 0.04281047f, -0.1384724f, 0.2145395f, 0.05335732f, 0.0284495f
},
{ // coefficient H
1.672403f, 1.303481f, 1.778589f, 2.611218f, 0.3510751f, 0.9492671f
},
{ // coefficient I
0.5897681f, 0.2453403f, 0.9159045f, 0.7082708f, 0.5421327f, 0.692946f
},
{ // radiance
0.005912801f, 0.006826679f, 0.01008544f, -0.02771812f, 0.3640405f, 0.2386599f
}
},
{ // turbidity 9
{ // coefficient A
-2.428587f, -3.240872f, -1.970987f, 0.03562238f, -1.50179f, -0.9859417f
},
{ // coefficient B
-1.538689f, -1.646067f, -2.124015f, 1.092427f, -0.940852f, -0.3827367f
},
{ // coefficient C
-9.076712f, 6.091797f, -2.894f, 0.6291668f, 1.000954f, 0.804093f
},
{ // coefficient D
9.46405f, -5.738481f, 3.85826f, -3.337396f, 4.599103f, 2.980678f
},
{ // coefficient E
-0.01503392f, -0.03098072f, -0.1449222f, -0.2817141f, -5.295662f, -2.015582f
},
{ // coefficient F
0.2373401f, 0.2698026f, 0.1903898f, 0.4799828f, -0.1111397f, -0.008947514f
},
{ // coefficient G
-2.770103e-06f, 0.03864212f, -0.1194017f, 0.167435f, 0.1040124f, 0.01696367f
},
{ // coefficient H
1.537508f, 1.286514f, 1.882566f, 1.943489f, 0.3185557f, 0.6489501f
},
{ // coefficient I
0.5959289f, 0.2461153f, 0.8764452f, 0.7728749f, 0.49594f, 0.7555598f
},
{ // radiance
0.005550967f, 0.005157051f, 0.01661696f, -0.05072656f, 0.3807009f, 0.2315489f
}
},
{ // turbidity 10
{ // coefficient A
-3.758956f, -4.635664f, -3.458011f, 0.9673527f, -1.808078f, -0.8399253f
},
{ // coefficient B
-2.159015f, -1.766046f, -3.65728f, 1.500425f, -1.089975f, -0.3832885f
},
{ // coefficient C
-7.207163f, 4.282669f, -1.484743f, -0.3604932f, 1.291838f, 0.6306417f
},
{ // coefficient D
7.516659f, -4.058908f, 2.264533f, -2.050346f, 5.144631f, 2.179775f
},
{ // coefficient E
-0.01643521f, -0.02908998f, -0.103312f, -0.3778381f, -5.047822f, -1.492762f
},
{ // coefficient F
0.2465725f, 0.1202684f, 0.4282323f, 0.2416298f, 0.04301183f, -0.1682763f
},
{ // coefficient G
-4.045068e-06f, 0.03167305f, -0.08387881f, 0.08439313f, 0.1426941f, 0.04109851f
},
{ // coefficient H
1.471688f, 1.450927f, 1.687334f, 1.458583f, 0.1187472f, 0.4221724f
},
{ // coefficient I
0.6687943f, 0.08171815f, 1.130604f, 0.5816071f, 0.6145551f, 0.7086504f
},
{ // radiance
0.005011118f, 0.002636752f, 0.02499879f, -0.07576617f, 0.3828189f, 0.2328116f
}
}
}
},
{ // 440.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.397312f, -1.561639f, -1.047473f, -1.298483f, -0.9485899f, -1.099082f
},
{ // coefficient B
-0.5327311f, -0.7024946f, -0.1152734f, -0.5788209f, -0.0738361f, -0.2001688f
},
{ // coefficient C
-5.456059f, -2.140322f, 2.275873f, 0.4906412f, 1.039247f, 0.7365077f
},
{ // coefficient D
5.777674f, 2.841761f, -3.020347f, 1.522576f, -0.4744931f, 0.1068489f
},
{ // coefficient E
-0.05111575f, -0.08846023f, -0.1739218f, -0.5650061f, -0.1822357f, -5.005434f
},
{ // coefficient F
0.4730804f, 0.5529005f, 0.2250142f, 1.544707f, 0.2284415f, 1.508145f
},
{ // coefficient G
0.0073755f, 0.006219344f, -0.02731776f, 0.04638003f, -0.04796962f, 0.08181497f
},
{ // coefficient H
3.032806f, 2.004212f, 6.659723f, -3.742019f, 4.363935f, 3.226865f
},
{ // coefficient I
0.518189f, 0.3701256f, 1.067495f, 0.2485397f, 0.8217971f, 0.4999418f
},
{ // radiance
0.009406889f, 0.01954373f, -0.04018205f, 0.1740051f, 0.135102f, 0.1365376f
}
},
{ // turbidity 2
{ // coefficient A
-1.416143f, -1.698183f, -0.9234085f, -1.265077f, -1.00821f, -1.07684f
},
{ // coefficient B
-0.5515006f, -0.8272914f, -0.02165404f, -0.5034231f, -0.1455232f, -0.1851501f
},
{ // coefficient C
-7.045898f, 0.9984388f, 0.06483039f, 1.196547f, 0.7347739f, 0.7341818f
},
{ // coefficient D
7.448161f, -0.2511235f, -0.8535145f, 0.3666463f, 0.02462433f, -0.002658571f
},
{ // coefficient E
-0.04504075f, -0.09262675f, -0.2497457f, -0.3960044f, -2.534587f, -5.001762f
},
{ // coefficient F
0.5055437f, 0.5681534f, 0.1695057f, 1.404008f, 0.308859f, 1.27539f
},
{ // coefficient G
-1.378949e-06f, 0.005377123f, -0.03410225f, 0.11293f, -0.06687136f, 0.127741f
},
{ // coefficient H
2.955475f, 1.631048f, 6.726925f, -2.644439f, 3.786007f, 3.223315f
},
{ // coefficient I
0.6184716f, 0.1137691f, 1.331197f, 0.1103356f, 0.9578369f, 0.5147659f
},
{ // radiance
0.009206049f, 0.02042313f, -0.0437738f, 0.1787089f, 0.1388614f, 0.1384128f
}
},
{ // turbidity 3
{ // coefficient A
-1.449526f, -1.74884f, -0.8369988f, -1.284515f, -0.9889637f, -1.09391f
},
{ // coefficient B
-0.5738159f, -0.9338946f, 0.1127967f, -0.5332365f, -0.09962819f, -0.2211349f
},
{ // coefficient C
-8.976383f, 0.0555746f, 0.8846958f, 0.5511261f, 0.6096776f, 0.6959659f
},
{ // coefficient D
9.275142f, 0.9451626f, -2.467908f, 1.702784f, -0.8472749f, 0.3423174f
},
{ // coefficient E
-0.03157845f, -0.06929558f, -0.08394011f, -0.6616281f, 0.7672924f, -5.004213f
},
{ // coefficient F
0.501197f, 0.5458365f, 0.1684067f, 1.349449f, 0.0742516f, 1.347201f
},
{ // coefficient G
-1.218363e-06f, 0.005505399f, -0.03352743f, 0.1269152f, 0.02113714f, 0.1198192f
},
{ // coefficient H
3.001006f, 0.8818793f, 7.957386f, -3.247373f, 4.990637f, 2.547517f
},
{ // coefficient I
0.566219f, 0.2135274f, 1.297777f, 0.2115827f, 0.8274585f, 0.6329489f
},
{ // radiance
0.009050889f, 0.02086396f, -0.04652916f, 0.1794829f, 0.1524222f, 0.1376563f
}
},
{ // turbidity 4
{ // coefficient A
-1.394462f, -2.234192f, -0.4063889f, -1.366924f, -0.9128947f, -1.162856f
},
{ // coefficient B
-0.5287733f, -1.437079f, 0.4920785f, -0.4557853f, 0.01980978f, -0.3606511f
},
{ // coefficient C
-8.095187f, -0.5734544f, 0.386552f, -0.6731417f, 0.5119874f, 0.7055795f
},
{ // coefficient D
8.076613f, 1.248687f, 0.2254008f, -0.3011452f, 0.08027541f, -0.002702137f
},
{ // coefficient E
-0.01275227f, -0.04136344f, -0.1618861f, 0.2413069f, -3.680159f, 0.4777294f
},
{ // coefficient F
0.191679f, 0.7671749f, 0.1950875f, 1.404821f, 0.6597416f, -0.7722331f
},
{ // coefficient G
0.04741296f, -0.01273584f, -0.08622479f, 0.1920278f, 0.03853339f, 0.3446509f
},
{ // coefficient H
3.14514f, 0.4751894f, 3.8369f, 6.394529f, 3.969131f, 1.186834f
},
{ // coefficient I
0.507176f, 0.4116995f, 1.108189f, 0.4219713f, 0.8351942f, 0.5118539f
},
{ // radiance
0.008656343f, 0.02210653f, -0.05212791f, 0.1836315f, 0.166304f, 0.1419649f
}
},
{ // turbidity 5
{ // coefficient A
-1.563353f, -1.948172f, -0.9155219f, -1.010898f, -1.159005f, -1.081675f
},
{ // coefficient B
-0.6916347f, -1.080669f, -0.1456265f, -0.04425431f, -0.4048136f, -0.2556981f
},
{ // coefficient C
-41.54766f, 10.82552f, -2.177275f, 0.306788f, 0.5385745f, 0.6544471f
},
{ // coefficient D
41.81703f, -10.34262f, 2.079788f, -0.6899235f, 0.3423651f, 0.4093649f
},
{ // coefficient E
-0.00567337f, -0.01196421f, -0.02154465f, -0.1891895f, 0.1474365f, -3.215201f
},
{ // coefficient F
0.43449f, 0.3895739f, 0.7743816f, 0.3015686f, 0.3516065f, 0.91164f
},
{ // coefficient G
0.001839829f, 0.05621831f, -0.2181642f, 0.4236422f, 0.00256426f, 0.140666f
},
{ // coefficient H
2.669278f, 1.34713f, 4.897704f, 1.901582f, 1.815518f, 1.869407f
},
{ // coefficient I
0.5021266f, 0.4561106f, 1.035464f, 0.3916662f, 0.7634079f, 0.6968119f
},
{ // radiance
0.008355466f, 0.02244975f, -0.05393078f, 0.1788056f, 0.1868417f, 0.1445069f
}
},
{ // turbidity 6
{ // coefficient A
-1.561484f, -2.30805f, -0.4886429f, -1.306115f, -1.016608f, -1.127637f
},
{ // coefficient B
-0.7153039f, -1.314851f, 0.08279799f, -0.175965f, -0.3429911f, -0.2854231f
},
{ // coefficient C
-16.23772f, -1.484346f, 2.432876f, -1.392008f, 0.565947f, 0.4933964f
},
{ // coefficient D
16.50713f, 1.928705f, -2.276257f, 0.3119302f, 0.9044333f, -0.003379287f
},
{ // coefficient E
-0.01394913f, -0.0216413f, -0.04799742f, -0.2268673f, 0.07387499f, -1.181893f
},
{ // coefficient F
0.4467465f, 0.2817711f, 0.936201f, 0.08336951f, 0.4267609f, 0.8164171f
},
{ // coefficient G
0.0009613464f, 0.07177671f, -0.2753669f, 0.5250725f, -0.04165623f, 0.1910509f
},
{ // coefficient H
2.564429f, 1.146387f, 4.784291f, 2.534962f, 1.452779f, 1.979338f
},
{ // coefficient I
0.5363746f, 0.3025595f, 1.180593f, 0.2800244f, 0.840956f, 0.6658885f
},
{ // radiance
0.008187585f, 0.02235434f, -0.05291187f, 0.1693688f, 0.2073941f, 0.1424517f
}
},
{ // turbidity 7
{ // coefficient A
-1.67531f, -2.071301f, -1.093503f, -0.7736128f, -1.259083f, -1.094148f
},
{ // coefficient B
-0.8020443f, -1.175325f, -0.4120496f, 0.2385498f, -0.5300975f, -0.3178958f
},
{ // coefficient C
-11.3613f, 1.789426f, -0.08598647f, -0.7867818f, 0.5418031f, 0.4126954f
},
{ // coefficient D
11.59861f, -1.455233f, 0.4564844f, -0.6009723f, 1.150966f, 0.3591106f
},
{ // coefficient E
-0.01871156f, -0.02855906f, -0.123198f, -0.1808566f, -0.7553821f, -1.415744f
},
{ // coefficient F
0.41514f, 0.3218745f, 0.7992132f, -0.008796429f, 0.5991f, 0.3908399f
},
{ // coefficient G
-1.940992e-06f, 0.07776969f, -0.2910745f, 0.541262f, -0.002344718f, 0.1806147f
},
{ // coefficient H
2.486322f, 1.431615f, 3.881502f, 2.991552f, 1.097939f, 1.584959f
},
{ // coefficient I
0.5256426f, 0.3478887f, 1.064658f, 0.4008143f, 0.7756612f, 0.6862009f
},
{ // radiance
0.007909229f, 0.02199365f, -0.05165975f, 0.1577725f, 0.2297489f, 0.1468516f
}
},
{ // turbidity 8
{ // coefficient A
-1.815485f, -2.22557f, -1.439532f, -0.559989f, -1.319304f, -1.097281f
},
{ // coefficient B
-0.9306778f, -1.213627f, -0.8666475f, 0.4437236f, -0.6466246f, -0.3336023f
},
{ // coefficient C
-12.49527f, 6.082472f, -2.734807f, 0.158232f, 0.5684685f, 0.1781708f
},
{ // coefficient D
12.71288f, -5.907562f, 3.355265f, -2.00011f, 2.05033f, 1.733079f
},
{ // coefficient E
-0.01497209f, -0.02446452f, -0.1311068f, -0.2859304f, -2.796053f, -1.378806f
},
{ // coefficient F
0.3576353f, 0.2113558f, 1.048703f, -0.5600631f, 1.001659f, -0.1870394f
},
{ // coefficient G
-2.464284e-06f, 0.07708802f, -0.2887025f, 0.5382793f, -0.01161356f, 0.1997578f
},
{ // coefficient H
2.32646f, 1.647556f, 3.419973f, 1.986387f, 0.9737235f, 1.456148f
},
{ // coefficient I
0.5655414f, 0.2416155f, 1.146322f, 0.3455121f, 0.8101246f, 0.6791796f
},
{ // radiance
0.007580566f, 0.02048421f, -0.04471863f, 0.1277026f, 0.273762f, 0.14976f
}
},
{ // turbidity 9
{ // coefficient A
-2.208666f, -1.967348f, -3.150304f, 0.4737094f, -1.706975f, -1.012042f
},
{ // coefficient B
-1.219745f, -0.9528057f, -2.193706f, 1.067875f, -0.9607442f, -0.3372525f
},
{ // coefficient C
-7.548669f, 4.17159f, -2.016165f, -0.1003861f, 0.7140098f, 0.0258945f
},
{ // coefficient D
7.68083f, -4.002838f, 2.374337f, -1.566752f, 3.981202f, 1.768224f
},
{ // coefficient E
-0.02012473f, -0.05074552f, -0.06793058f, -1.217264f, -3.521433f, -1.05999f
},
{ // coefficient F
0.3627866f, 0.1581866f, 0.9623811f, -0.4270598f, 0.630882f, -0.1299077f
},
{ // coefficient G
-2.906182e-06f, 0.05920106f, -0.2195022f, 0.3968055f, 0.05140937f, 0.1790337f
},
{ // coefficient H
2.147138f, 1.981564f, 2.736885f, 1.234108f, 0.743512f, 1.091012f
},
{ // coefficient I
0.6340095f, 0.1023257f, 1.252161f, 0.3105789f, 0.8074305f, 0.6873854f
},
{ // radiance
0.007101943f, 0.01675094f, -0.02761544f, 0.07078506f, 0.3397147f, 0.1557184f
}
},
{ // turbidity 10
{ // coefficient A
-2.912541f, -1.567383f, -6.058613f, 1.347177f, -1.810783f, -0.9801965f
},
{ // coefficient B
-1.677524f, -0.8279686f, -3.509207f, 0.9821888f, -1.027512f, -0.3375841f
},
{ // coefficient C
-3.843452f, 1.69094f, -0.631287f, -0.9242155f, 1.216071f, -0.3004804f
},
{ // coefficient D
3.954207f, -1.413268f, -0.04558316f, 0.5851576f, 5.270108f, 1.385754f
},
{ // coefficient E
-0.03414327f, -0.1216792f, 0.06168891f, -1.897421f, -3.887149f, -0.683454f
},
{ // coefficient F
0.294589f, 0.07332798f, 0.8153413f, -0.02729487f, 0.06193256f, 0.05827734f
},
{ // coefficient G
-4.245832e-06f, 0.04862765f, -0.1686845f, 0.2866724f, 0.0277461f, 0.2268309f
},
{ // coefficient H
1.934337f, 2.034766f, 2.670227f, 0.7306238f, 0.1484756f, 1.028696f
},
{ // coefficient I
0.7258748f, -0.05041561f, 1.362906f, 0.2792136f, 0.8033249f, 0.6797371f
},
{ // radiance
0.006525444f, 0.01090411f, -0.001995302f, -0.008176097f, 0.4124321f, 0.1714881f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.34526f, -1.573525f, -1.013665f, -1.173191f, -1.003507f, -1.071959f
},
{ // coefficient B
-0.5085528f, -0.7133524f, -0.1731419f, -0.3964683f, -0.1124908f, -0.1567262f
},
{ // coefficient C
-6.052009f, -1.671346f, 2.043423f, 1.097258f, 1.030642f, 0.9923029f
},
{ // coefficient D
6.60459f, 2.693205f, -2.919634f, 1.509973f, -0.5357088f, 0.145716f
},
{ // coefficient E
-0.04300752f, -0.07828763f, -0.1020807f, -0.5675641f, 0.6114201f, -4.742907f
},
{ // coefficient F
0.4745234f, 0.2915699f, 0.5467619f, 0.9815927f, 0.1628414f, 0.728097f
},
{ // coefficient G
0.006591457f, 0.01795879f, -0.04041481f, 0.06962319f, -0.05701497f, 0.0677036f
},
{ // coefficient H
2.595142f, 1.469445f, 5.853992f, -3.35821f, 4.541126f, 3.9193f
},
{ // coefficient I
0.5013653f, 0.4927472f, 0.5272064f, 0.4263117f, 0.8084996f, 0.4999529f
},
{ // radiance
0.01111702f, 0.01783998f, -0.01949635f, 0.112497f, 0.299333f, 0.2864943f
}
},
{ // turbidity 2
{ // coefficient A
-1.399982f, -1.602839f, -0.9587294f, -1.166832f, -1.023178f, -1.071426f
},
{ // coefficient B
-0.5538549f, -0.7835806f, -0.06134134f, -0.4057833f, -0.1351631f, -0.1726491f
},
{ // coefficient C
-6.983148f, 1.200264f, 0.223403f, 1.461335f, 0.9944251f, 1.048949f
},
{ // coefficient D
7.513966f, -0.145429f, -0.8115395f, 0.3165401f, 0.02103341f, -0.00262484f
},
{ // coefficient E
-0.03358417f, -0.07431927f, -0.2344054f, -0.3245858f, -2.460318f, -4.993124f
},
{ // coefficient F
0.3667027f, 0.5374263f, 0.2018782f, 1.199323f, 0.005136074f, 0.6621513f
},
{ // coefficient G
0.01158837f, 0.0009505505f, -0.04192332f, 0.1100274f, -0.05267951f, 0.07801931f
},
{ // coefficient H
2.49968f, 1.096167f, 6.331499f, -2.727373f, 4.097235f, 3.22775f
},
{ // coefficient I
0.5598537f, 0.2185838f, 1.353587f, 0.05329788f, 0.9388837f, 0.5363265f
},
{ // radiance
0.01099681f, 0.01809609f, -0.02099087f, 0.1131858f, 0.3057413f, 0.2866962f
}
},
{ // turbidity 3
{ // coefficient A
-1.362723f, -1.824466f, -0.6435267f, -1.400277f, -0.9044866f, -1.115618f
},
{ // coefficient B
-0.5257224f, -0.987779f, 0.1629762f, -0.5150235f, -0.07979493f, -0.2148f
},
{ // coefficient C
-13.2513f, 4.631069f, -1.452097f, 1.752017f, 0.8298122f, 1.109081f
},
{ // coefficient D
13.74641f, -3.630521f, 0.8028761f, -0.1699743f, -0.05604698f, 0.2661209f
},
{ // coefficient E
-0.01971887f, -0.04340325f, -0.09611017f, -0.3411643f, -1.433761f, -5.008111f
},
{ // coefficient F
0.4480379f, 0.391345f, 0.4267105f, 0.8465596f, 0.1011757f, 0.6333048f
},
{ // coefficient G
-9.588735e-07f, 0.03415899f, -0.1023573f, 0.2127075f, -0.07175544f, 0.09274718f
},
{ // coefficient H
2.679516f, 0.47468f, 6.652106f, -2.235971f, 4.243075f, 2.494529f
},
{ // coefficient I
0.5628531f, 0.2022772f, 1.406057f, 0.04728189f, 0.9588179f, 0.5890657f
},
{ // radiance
0.01070017f, 0.01818573f, -0.02159895f, 0.1084494f, 0.322918f, 0.2835407f
}
},
{ // turbidity 4
{ // coefficient A
-1.389611f, -1.893851f, -0.7015617f, -1.279075f, -0.9744212f, -1.093264f
},
{ // coefficient B
-0.5579886f, -1.116122f, 0.1884526f, -0.4794032f, -0.09899254f, -0.2115594f
},
{ // coefficient C
-10.45229f, 2.111553f, -0.1660273f, 0.9037342f, 0.94746f, 0.9876876f
},
{ // coefficient D
10.92342f, -0.8775468f, -0.8785899f, 0.6225399f, -0.2536639f, 0.07531309f
},
{ // coefficient E
-0.02282772f, -0.05640158f, -0.05471987f, -0.659227f, 0.5390432f, -5.003648f
},
{ // coefficient F
0.4054079f, 0.5399894f, 0.1787856f, 0.9474313f, -0.08465488f, 0.671956f
},
{ // coefficient G
0.002781558f, 0.01929352f, -0.0995887f, 0.2524811f, -0.03140532f, 0.1111699f
},
{ // coefficient H
2.578484f, -0.05585557f, 7.207213f, -2.485658f, 4.512343f, 2.412349f
},
{ // coefficient I
0.5372706f, 0.2870402f, 1.277933f, 0.2050401f, 0.8530654f, 0.6482966f
},
{ // radiance
0.01032234f, 0.01871074f, -0.02469404f, 0.1069731f, 0.338045f, 0.2864479f
}
},
{ // turbidity 5
{ // coefficient A
-1.410458f, -1.817258f, -0.8920534f, -1.048633f, -1.094223f, -1.078162f
},
{ // coefficient B
-0.5905263f, -0.9848495f, -0.08835804f, -0.1110825f, -0.3084727f, -0.2176197f
},
{ // coefficient C
-41.46328f, 10.89217f, -2.111594f, 0.543871f, 0.9739595f, 1.088756f
},
{ // coefficient D
41.90254f, -10.28029f, 2.083599f, -0.6707296f, 0.397214f, 0.4329413f
},
{ // coefficient E
-0.005607421f, -0.01221684f, -0.015078f, -0.1825365f, 0.1511647f, -3.20949f
},
{ // coefficient F
0.4437719f, 0.3317008f, 0.6993567f, 0.2307221f, 0.2788218f, 0.8364785f
},
{ // coefficient G
-1.948391e-06f, 0.05060062f, -0.1987686f, 0.4056218f, -0.0627918f, 0.07689785f
},
{ // coefficient H
2.496743f, 1.198443f, 4.793539f, 1.886912f, 1.866444f, 1.943182f
},
{ // coefficient I
0.5031933f, 0.4449482f, 1.072586f, 0.3579576f, 0.7995203f, 0.7150615f
},
{ // radiance
0.009893912f, 0.01844953f, -0.023585f, 0.09439551f, 0.3642566f, 0.2829554f
}
},
{ // turbidity 6
{ // coefficient A
-1.455846f, -2.222251f, -0.4536196f, -1.277343f, -1.029833f, -1.095935f
},
{ // coefficient B
-0.6322333f, -1.233356f, 0.1457295f, -0.2046024f, -0.2979843f, -0.2211775f
},
{ // coefficient C
-16.18289f, -1.454954f, 2.483934f, -1.22972f, 0.8742883f, 0.9320585f
},
{ // coefficient D
16.56258f, 1.957432f, -2.24455f, 0.3977256f, 1.03771f, 0.08955212f
},
{ // coefficient E
-0.01280603f, -0.01730555f, -0.04593767f, -0.2209195f, 0.1350257f, -1.113388f
},
{ // coefficient F
0.4130359f, 0.2418496f, 0.8967621f, 0.06033235f, 0.4066094f, 0.7909911f
},
{ // coefficient G
-9.061644e-07f, 0.07240262f, -0.2712877f, 0.5267707f, -0.1376375f, 0.1393848f
},
{ // coefficient H
2.461391f, 1.061939f, 4.732142f, 2.536445f, 1.500753f, 2.057146f
},
{ // coefficient I
0.5315915f, 0.3148474f, 1.188839f, 0.2759606f, 0.8676699f, 0.6699739f
},
{ // radiance
0.0096504f, 0.01877062f, -0.02542361f, 0.09189004f, 0.372061f, 0.2856265f
}
},
{ // turbidity 7
{ // coefficient A
-1.571831f, -2.235398f, -0.6944677f, -1.133391f, -1.057051f, -1.096695f
},
{ // coefficient B
-0.7466353f, -1.371434f, -0.07243136f, -0.09811942f, -0.3424797f, -0.2745143f
},
{ // coefficient C
-12.26063f, 1.787996f, -0.6193104f, 0.2312721f, 0.9536822f, 1.07792f
},
{ // coefficient D
12.65995f, -1.073965f, 0.956744f, -1.100585f, 1.034064f, -0.0008435082f
},
{ // coefficient E
-0.01514083f, -0.0250353f, -0.06317194f, -0.3655097f, 0.6413292f, -3.715956f
},
{ // coefficient F
0.3639653f, 0.2143584f, 0.8881508f, 0.002268787f, 0.1165363f, 0.51979f
},
{ // coefficient G
-2.504287e-06f, 0.07703453f, -0.2783301f, 0.5074358f, -0.04248128f, 0.1171467f
},
{ // coefficient H
2.204454f, 0.5207181f, 4.224532f, 1.512457f, 1.592803f, 1.236912f
},
{ // coefficient I
0.5468861f, 0.2714312f, 1.192802f, 0.3165593f, 0.7875911f, 0.6915287f
},
{ // radiance
0.009322628f, 0.01810055f, -0.02246559f, 0.07543314f, 0.3958251f, 0.2842134f
}
},
{ // turbidity 8
{ // coefficient A
-1.711623f, -2.351937f, -1.141314f, -0.7950557f, -1.182289f, -1.080579f
},
{ // coefficient B
-0.9023881f, -1.340391f, -0.6670322f, 0.2489854f, -0.5192524f, -0.2918654f
},
{ // coefficient C
-9.799257f, 6.329976f, -3.262632f, 0.9966347f, 0.9672941f, 0.9499317f
},
{ // coefficient D
10.2204f, -6.054139f, 4.551521f, -3.391737f, 3.091448f, 2.856082f
},
{ // coefficient E
-0.01708447f, -0.02843868f, -0.08614941f, -0.7235716f, -4.193043f, -2.256007f
},
{ // coefficient F
0.3309925f, 0.2298301f, 0.8058038f, -0.2155609f, 0.4051469f, 0.0137775f
},
{ // coefficient G
-3.241111e-06f, 0.07698142f, -0.2791051f, 0.5126659f, -0.05708309f, 0.1012914f
},
{ // coefficient H
1.932295f, 1.1098f, 3.084281f, 1.452322f, 0.9386723f, 0.9716489f
},
{ // coefficient I
0.5844205f, 0.1863187f, 1.236638f, 0.2882812f, 0.8208828f, 0.683275f
},
{ // radiance
0.008842659f, 0.01604245f, -0.01397353f, 0.04233011f, 0.435827f, 0.2789888f
}
},
{ // turbidity 9
{ // coefficient A
-1.964781f, -2.352213f, -2.560247f, 0.07281064f, -1.444446f, -1.039666f
},
{ // coefficient B
-1.162652f, -1.199539f, -1.883591f, 0.7835433f, -0.7662151f, -0.3171617f
},
{ // coefficient C
-6.074737f, 4.248376f, -1.916723f, 0.1380748f, 1.364103f, 0.729592f
},
{ // coefficient D
6.497797f, -3.876965f, 2.598994f, -2.06558f, 4.930232f, 2.813846f
},
{ // coefficient E
-0.0259566f, -0.06837923f, -0.02372381f, -1.696648f, -4.455564f, -1.56123f
},
{ // coefficient F
0.3222936f, 0.1764412f, 0.8442578f, -0.4575316f, 0.6735458f, -0.4128303f
},
{ // coefficient G
-3.220871e-06f, 0.0551481f, -0.2063113f, 0.3840004f, -0.004052978f, 0.1004149f
},
{ // coefficient H
1.685891f, 1.442736f, 2.359942f, 0.9397056f, 0.3194173f, 0.6653613f
},
{ // coefficient I
0.6501551f, 0.06979575f, 1.287838f, 0.3103904f, 0.7842959f, 0.6980277f
},
{ // radiance
0.008169168f, 0.01265551f, 0.0004076399f, -0.007877623f, 0.4830048f, 0.2768485f
}
},
{ // turbidity 10
{ // coefficient A
-2.999915f, -1.673162f, -5.775475f, 1.241869f, -1.580126f, -0.9732917f
},
{ // coefficient B
-1.72174f, -1.056666f, -3.2397f, 0.6671222f, -0.7852349f, -0.1939297f
},
{ // coefficient C
-4.796494f, 3.209766f, -1.356432f, -0.4156007f, 1.749569f, 0.2709367f
},
{ // coefficient D
5.021888f, -2.559187f, 1.09995f, -0.8167921f, 5.98799f, 1.35299f
},
{ // coefficient E
-0.0216325f, -0.07456364f, -0.09497509f, -1.037816f, -5.075133f, -0.9040352f
},
{ // coefficient F
0.3309183f, 0.06215063f, 0.7121875f, 0.05950065f, -0.04693815f, 0.005181217f
},
{ // coefficient G
-4.051804e-06f, 0.04469425f, -0.1656838f, 0.3114868f, -0.05106759f, 0.2302437f
},
{ // coefficient H
1.645464f, 1.242757f, 2.553396f, 0.6414114f, -0.9755925f, 1.5065f
},
{ // coefficient I
0.714589f, -0.0158059f, 1.286227f, 0.3677503f, 0.7440092f, 0.6840186f
},
{ // radiance
0.007383449f, 0.007340131f, 0.02094279f, -0.07138907f, 0.5217098f, 0.280381f
}
}
}
},
{ // 480.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.255072f, -1.344988f, -1.073402f, -1.183711f, -1.018668f, -1.080081f
},
{ // coefficient B
-0.350117f, -0.436588f, -0.139526f, -0.4011868f, -0.08561816f, -0.1746769f
},
{ // coefficient C
-5.952795f, -1.750961f, 2.839003f, 0.735874f, 1.030241f, 1.174843f
},
{ // coefficient D
6.529723f, 2.262943f, -2.827315f, 1.479436f, -0.4461392f, 0.0938945f
},
{ // coefficient E
-0.06362962f, -0.1049983f, -0.188209f, -0.7268596f, 0.3940284f, -4.893194f
},
{ // coefficient F
0.6436422f, 0.3754288f, 1.054349f, 1.133998f, 1.220382f, 1.162505f
},
{ // coefficient G
0.00774527f, 0.05298712f, -0.0897633f, 0.1205942f, -0.1391238f, 0.1563406f
},
{ // coefficient H
3.657426f, 3.402851f, 5.604794f, -4.23297f, 5.771847f, 2.6068f
},
{ // coefficient I
0.5329375f, 0.3874229f, 0.674051f, 1.060586f, 0.562718f, 0.499999f
},
{ // radiance
0.01367487f, 0.01998477f, -0.02747378f, 0.2012913f, 0.1065803f, 0.1249226f
}
},
{ // turbidity 2
{ // coefficient A
-1.26843f, -1.45978f, -0.8680288f, -1.277439f, -0.9964896f, -1.083449f
},
{ // coefficient B
-0.3752425f, -0.5882818f, 0.1148721f, -0.4550135f, -0.09832891f, -0.1686372f
},
{ // coefficient C
-8.504068f, 1.800729f, 0.3161508f, 1.088681f, 0.9746026f, 1.003727f
},
{ // coefficient D
9.021017f, -0.9674514f, -1.52487f, 1.55237f, -0.9125638f, 0.4085513f
},
{ // coefficient E
-0.04092613f, -0.08308819f, -0.1921031f, -0.6184442f, -0.745717f, -4.725304f
},
{ // coefficient F
0.6553965f, 0.4453337f, 0.6496299f, 1.386667f, 0.9111352f, 1.112591f
},
{ // coefficient G
0.008607147f, 0.04086836f, 0.0004422347f, 0.1035135f, -0.07027011f, 0.185524f
},
{ // coefficient H
3.444138f, 1.853978f, 8.241268f, -3.331179f, 4.669829f, 3.038195f
},
{ // coefficient I
0.5600385f, 0.2818975f, 0.9821338f, 0.5236122f, 0.7455938f, 0.5969305f
},
{ // radiance
0.01319623f, 0.02233282f, -0.03838862f, 0.2216885f, 0.1032539f, 0.1308019f
}
},
{ // turbidity 3
{ // coefficient A
-1.290677f, -1.535002f, -0.7903104f, -1.278947f, -1.006667f, -1.086931f
},
{ // coefficient B
-0.3901158f, -0.6667464f, 0.1563438f, -0.3694875f, -0.1519095f, -0.1774734f
},
{ // coefficient C
-9.520695f, 0.02735758f, -0.2022567f, 0.8111792f, 0.8774664f, 0.8989024f
},
{ // coefficient D
9.828711f, 0.9423936f, -1.02667f, 0.4993027f, -0.1909283f, 0.05536469f
},
{ // coefficient E
-0.03154131f, -0.0828724f, 0.009063977f, -1.096772f, 3.756189f, -3.474034f
},
{ // coefficient F
0.5939314f, 0.6775029f, 0.3942033f, 1.008837f, 0.9311862f, 0.9736834f
},
{ // coefficient G
0.0268684f, 0.004575744f, -0.0124198f, 0.2920972f, -0.09327427f, 0.2325787f
},
{ // coefficient H
3.612397f, 1.432097f, 7.980026f, -0.7314209f, 3.499747f, 2.809427f
},
{ // coefficient I
0.5140176f, 0.3872415f, 1.066191f, 0.317944f, 0.8574583f, 0.6246481f
},
{ // radiance
0.01265118f, 0.02391819f, -0.04721677f, 0.2352182f, 0.1174476f, 0.1303906f
}
},
{ // turbidity 4
{ // coefficient A
-1.346571f, -1.607315f, -0.8259129f, -1.204471f, -1.035718f, -1.087946f
},
{ // coefficient B
-0.4451276f, -0.741636f, 0.09305732f, -0.262058f, -0.2116415f, -0.1841898f
},
{ // coefficient C
-23.73159f, -0.5233286f, 4.86432f, -2.637261f, 1.288642f, 0.4491757f
},
{ // coefficient D
24.08962f, 1.169215f, -5.585537f, 3.057335f, -0.3834162f, 0.1691479f
},
{ // coefficient E
-0.01225268f, -0.02350907f, -0.04428624f, -0.1226916f, -0.0948805f, 0.1022577f
},
{ // coefficient F
0.5720568f, 0.4942922f, 0.7540493f, 0.5657925f, 0.7404996f, 0.9202996f
},
{ // coefficient G
0.01636363f, 0.03544231f, -0.1060506f, 0.4446409f, -0.06538922f, 0.2926112f
},
{ // coefficient H
3.288957f, 1.659598f, 6.548781f, 1.136957f, 2.336877f, 2.844475f
},
{ // coefficient I
0.5104502f, 0.3959771f, 1.138009f, 0.2881385f, 0.861203f, 0.645262f
},
{ // radiance
0.01204726f, 0.0274513f, -0.06437791f, 0.2605947f, 0.1298129f, 0.1378516f
}
},
{ // turbidity 5
{ // coefficient A
-1.381782f, -1.776157f, -0.8381948f, -1.125194f, -1.095144f, -1.062019f
},
{ // coefficient B
-0.4914069f, -0.9226583f, 0.1229315f, -0.2647028f, -0.2442208f, -0.1765309f
},
{ // coefficient C
-48.13999f, 13.89662f, -3.370527f, 0.6942391f, 0.4787687f, 0.5423897f
},
{ // coefficient D
48.37634f, -12.9249f, 1.82551f, 1.389102f, -2.70794f, 3.783099f
},
{ // coefficient E
-0.005296025f, -0.01480095f, -0.009728251f, -0.2216447f, 0.6129627f, -5.007038f
},
{ // coefficient F
0.5571609f, 0.5073308f, 0.8572929f, 0.1356485f, 0.7213058f, 0.7376299f
},
{ // coefficient G
0.01649034f, 0.04585849f, -0.1855299f, 0.5812792f, -0.06034254f, 0.3034085f
},
{ // coefficient H
3.203385f, 0.7385206f, 7.471741f, -0.5073865f, 2.798703f, 2.733411f
},
{ // coefficient I
0.5048069f, 0.4280887f, 1.13319f, 0.267549f, 0.8787214f, 0.655507f
},
{ // radiance
0.01125691f, 0.03031033f, -0.0785607f, 0.2797308f, 0.1411425f, 0.1486601f
}
},
{ // turbidity 6
{ // coefficient A
-1.390263f, -1.935801f, -0.7547378f, -1.157324f, -1.065566f, -1.095972f
},
{ // coefficient B
-0.5185474f, -0.9953289f, 0.02590987f, -0.1198742f, -0.3139616f, -0.2178745f
},
{ // coefficient C
-16.30741f, -1.483271f, 2.480507f, -1.440574f, 0.5715306f, 0.4681537f
},
{ // coefficient D
16.54109f, 1.913745f, -2.271835f, 0.3070751f, 0.8053761f, 0.1235458f
},
{ // coefficient E
-0.01499201f, -0.0251816f, -0.0525962f, -0.1807577f, 0.1396364f, -1.084613f
},
{ // coefficient F
0.5661787f, 0.4593343f, 0.9001709f, 0.04059237f, 0.4009778f, 1.006455f
},
{ // coefficient G
0.01452544f, 0.04285617f, -0.2024986f, 0.6210184f, 0.005681033f, 0.2754677f
},
{ // coefficient H
3.046179f, 1.448437f, 4.958191f, 2.425946f, 1.354013f, 2.239616f
},
{ // coefficient I
0.50267f, 0.450931f, 1.03694f, 0.3828327f, 0.7861375f, 0.6954839f
},
{ // radiance
0.01087451f, 0.03134174f, -0.08308676f, 0.2793109f, 0.1610134f, 0.150229f
}
},
{ // turbidity 7
{ // coefficient A
-1.45315f, -1.925518f, -1.134311f, -0.8094396f, -1.227889f, -1.06545f
},
{ // coefficient B
-0.5790574f, -1.010928f, -0.275471f, 0.1096826f, -0.4522793f, -0.2160134f
},
{ // coefficient C
-12.43832f, 1.513827f, -0.2443212f, -0.5117072f, 0.05338266f, 0.3413994f
},
{ // coefficient D
12.63425f, -0.9219873f, 0.03845396f, -0.1915992f, 1.074118f, 0.4950303f
},
{ // coefficient E
-0.01892784f, -0.03776925f, -0.1074645f, -0.1991693f, 0.08733709f, -1.077199f
},
{ // coefficient F
0.4979177f, 0.4692652f, 1.066657f, -0.5943341f, 1.005448f, 0.3654185f
},
{ // coefficient G
0.01035899f, 0.04083023f, -0.2324557f, 0.6791986f, -0.01725176f, 0.3125819f
},
{ // coefficient H
2.982682f, 1.303495f, 4.996897f, 1.332014f, 1.32611f, 1.998536f
},
{ // coefficient I
0.5056256f, 0.424515f, 1.052268f, 0.3606306f, 0.8063751f, 0.6865917f
},
{ // radiance
0.01035757f, 0.03176437f, -0.0848637f, 0.2674373f, 0.1974534f, 0.1533206f
}
},
{ // turbidity 8
{ // coefficient A
-1.594869f, -1.980952f, -1.766081f, -0.3742071f, -1.380378f, -1.088465f
},
{ // coefficient B
-0.736676f, -0.991092f, -0.899515f, 0.4678023f, -0.6406403f, -0.297119f
},
{ // coefficient C
-12.52117f, 6.178699f, -2.712687f, 0.06371403f, 0.7018678f, 0.1411657f
},
{ // coefficient D
12.7503f, -5.888582f, 3.263201f, -2.016941f, 2.032023f, 1.879601f
},
{ // coefficient E
-0.01697659f, -0.03099336f, -0.1840688f, -0.2723239f, -2.393487f, -1.310304f
},
{ // coefficient F
0.4892543f, 0.3859266f, 1.30541f, -1.446876f, 1.930827f, -0.704574f
},
{ // coefficient G
0.002831848f, 0.04511764f, -0.2675879f, 0.7533604f, -0.1260608f, 0.3313185f
},
{ // coefficient H
2.542039f, 1.839601f, 3.647246f, 1.13176f, 0.7634327f, 1.264266f
},
{ // coefficient I
0.5457358f, 0.2791527f, 1.168203f, 0.2537537f, 0.8870918f, 0.6534482f
},
{ // radiance
0.009649434f, 0.03087937f, -0.08067259f, 0.2353243f, 0.2553337f, 0.1608814f
}
},
{ // turbidity 9
{ // coefficient A
-2.229949f, -1.995509f, -3.174785f, 0.4882832f, -1.709118f, -0.9999977f
},
{ // coefficient B
-1.153733f, -0.8778717f, -2.10513f, 1.097365f, -0.9374505f, -0.2777886f
},
{ // coefficient C
-7.505783f, 4.191031f, -2.06223f, -0.1866214f, 0.6559f, 0.009586687f
},
{ // coefficient D
7.718722f, -3.968507f, 2.391786f, -1.567231f, 3.97786f, 1.766813f
},
{ // coefficient E
-0.02407129f, -0.05176872f, -0.04217147f, -1.216396f, -3.525762f, -1.065121f
},
{ // coefficient F
0.417582f, 0.1960357f, 0.9727297f, -0.439128f, 0.6202828f, -0.1325963f
},
{ // coefficient G
4.559186e-05f, 0.03755797f, -0.1540078f, 0.4541998f, 0.07863257f, 0.2512239f
},
{ // coefficient H
2.157318f, 1.975436f, 2.691773f, 1.167636f, 0.6882512f, 1.055043f
},
{ // coefficient I
0.6337774f, 0.1077053f, 1.235189f, 0.3003642f, 0.8161696f, 0.6874661f
},
{ // radiance
0.008869346f, 0.02627497f, -0.05891488f, 0.1570602f, 0.3504762f, 0.1724148f
}
},
{ // turbidity 10
{ // coefficient A
-2.932652f, -1.591732f, -6.08576f, 1.335204f, -1.800155f, -0.9843476f
},
{ // coefficient B
-1.609416f, -0.7579896f, -3.429651f, 1.035931f, -1.010684f, -0.3014031f
},
{ // coefficient C
-3.825171f, 1.711976f, -0.6780006f, -1.008915f, 1.16029f, -0.3167912f
},
{ // coefficient D
3.969752f, -1.369988f, -0.01902709f, 0.5915758f, 5.26919f, 1.384556f
},
{ // coefficient E
-0.03638195f, -0.1107557f, 0.06566034f, -1.907371f, -3.895543f, -0.6881572f
},
{ // coefficient F
0.3557402f, 0.1237841f, 0.8329946f, -0.03568938f, 0.05274946f, 0.05617519f
},
{ // coefficient G
-4.379735e-06f, 0.01548121f, -0.1012845f, 0.3295382f, 0.03971324f, 0.2805263f
},
{ // coefficient H
1.927804f, 2.024749f, 2.617589f, 0.654648f, 0.08769492f, 0.9930291f
},
{ // coefficient I
0.727838f, -0.04315379f, 1.342663f, 0.2435447f, 0.8252336f, 0.6783964f
},
{ // radiance
0.00810536f, 0.01756907f, -0.02001928f, 0.03536541f, 0.4692392f, 0.1926373f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.257325f, -1.346061f, -1.073983f, -1.184707f, -1.021623f, -1.087791f
},
{ // coefficient B
-0.3441598f, -0.4331978f, -0.1372222f, -0.3992543f, -0.08422241f, -0.1754873f
},
{ // coefficient C
-5.951667f, -1.749638f, 2.840388f, 0.7374377f, 1.032293f, 1.178006f
},
{ // coefficient D
6.530618f, 2.263845f, -2.826616f, 1.479838f, -0.4460168f, 0.0938685f
},
{ // coefficient E
-0.05773033f, -0.1013807f, -0.1866609f, -0.7263903f, 0.3941239f, -4.893184f
},
{ // coefficient F
0.643799f, 0.3755084f, 1.054193f, 1.133619f, 1.219939f, 1.162164f
},
{ // coefficient G
0.008453838f, 0.05287214f, -0.08989038f, 0.1199206f, -0.1420884f, 0.1535543f
},
{ // coefficient H
3.657343f, 3.402954f, 5.604984f, -4.232702f, 5.772239f, 2.60746f
},
{ // coefficient I
0.5329715f, 0.3874047f, 0.6739811f, 1.060412f, 0.5623142f, 0.4997706f
},
{ // radiance
0.01531497f, 0.02090118f, -0.01665041f, 0.1626765f, 0.2602854f, 0.273935f
}
},
{ // turbidity 2
{ // coefficient A
-1.251295f, -1.374134f, -0.9862389f, -1.140461f, -1.049493f, -1.063961f
},
{ // coefficient B
-0.3651612f, -0.5079738f, -0.02441831f, -0.3212487f, -0.1256523f, -0.1336751f
},
{ // coefficient C
-9.950987f, 2.333109f, 0.2657376f, 1.547332f, 1.205849f, 1.173271f
},
{ // coefficient D
10.58444f, -1.251359f, -1.054016f, 0.9031281f, -0.3649526f, 0.1016777f
},
{ // coefficient E
-0.03065334f, -0.0780208f, -0.08576871f, -0.8421239f, 0.8075413f, -4.093576f
},
{ // coefficient F
0.597956f, 0.3182768f, 0.7356127f, 1.166595f, 0.3421308f, 0.4559998f
},
{ // coefficient G
0.01817303f, 0.03746599f, -0.04711349f, 0.1643076f, -0.1173508f, 0.1519017f
},
{ // coefficient H
3.13246f, 1.96394f, 6.677884f, -2.534096f, 4.535809f, 4.153208f
},
{ // coefficient I
0.5458738f, 0.273318f, 1.206629f, 0.1884034f, 0.9497555f, 0.5324846f
},
{ // radiance
0.01500055f, 0.02258405f, -0.02517423f, 0.1780058f, 0.2604774f, 0.2803331f
}
},
{ // turbidity 3
{ // coefficient A
-1.271751f, -1.472034f, -0.8587401f, -1.221875f, -1.008105f, -1.082262f
},
{ // coefficient B
-0.3837936f, -0.625976f, 0.1099859f, -0.3659269f, -0.1136472f, -0.1513279f
},
{ // coefficient C
-10.52025f, -0.5356154f, 0.3258521f, 0.9953072f, 1.248935f, 1.052383f
},
{ // coefficient D
11.02826f, 1.7135f, -1.301658f, 0.8481237f, -0.3865165f, 0.2125686f
},
{ // coefficient E
-0.02411879f, -0.0608997f, -0.04452458f, -0.5952235f, 1.714619f, -0.5028093f
},
{ // coefficient F
0.5019991f, 0.4750716f, 0.6461336f, 1.016442f, -0.1216223f, 0.8694047f
},
{ // coefficient G
0.03094786f, 0.006622428f, -0.02947489f, 0.225994f, -0.02436842f, 0.1099128f
},
{ // coefficient H
3.138107f, 1.224931f, 7.603712f, -1.925411f, 4.227455f, 3.634467f
},
{ // coefficient I
0.5059631f, 0.4198087f, 1.091511f, 0.3654292f, 0.7030495f, 0.7206727f
},
{ // radiance
0.01435727f, 0.02473634f, -0.03651251f, 0.1966627f, 0.2645889f, 0.2894029f
}
},
{ // turbidity 4
{ // coefficient A
-1.310438f, -1.598864f, -0.7834866f, -1.24491f, -1.010078f, -1.086611f
},
{ // coefficient B
-0.4332417f, -0.7787419f, 0.189074f, -0.3668018f, -0.1593623f, -0.1529597f
},
{ // coefficient C
-28.12457f, 4.189377f, 1.102853f, -0.2083168f, 1.747411f, 0.4548984f
},
{ // coefficient D
28.59054f, -3.122921f, -1.842558f, 1.051092f, -0.06961852f, 0.2836062f
},
{ // coefficient E
-0.007241887f, -0.01896105f, -0.03985f, -0.1483362f, -0.1503091f, 0.4168066f
},
{ // coefficient F
0.4123186f, 0.6071985f, 0.6423282f, 0.406621f, 0.5439839f, 0.3953841f
},
{ // coefficient G
0.02839263f, 0.0199966f, -0.1131231f, 0.462336f, -0.1874287f, 0.2451427f
},
{ // coefficient H
2.866138f, 0.7039076f, 7.222493f, -0.1971682f, 2.074758f, 3.799065f
},
{ // coefficient I
0.5063521f, 0.4125456f, 1.177285f, 0.217889f, 0.9133302f, 0.6288711f
},
{ // radiance
0.01368791f, 0.02602874f, -0.04462666f, 0.2008996f, 0.2978971f, 0.2872899f
}
},
{ // turbidity 5
{ // coefficient A
-1.338369f, -1.78578f, -0.6972735f, -1.307129f, -0.9583414f, -1.11404f
},
{ // coefficient B
-0.4683873f, -0.9773633f, 0.2786524f, -0.4728119f, -0.07928834f, -0.2132678f
},
{ // coefficient C
-38.5811f, 6.470331f, 0.00401135f, 0.02639424f, 1.447497f, 1.030931f
},
{ // coefficient D
38.98823f, -5.147348f, -1.686706f, 2.633155f, -2.797571f, 2.871507f
},
{ // coefficient E
-0.005111812f, -0.01593592f, -0.004966516f, -0.2357439f, 0.8553597f, -5.013319f
},
{ // coefficient F
0.4638476f, 0.417572f, 0.8368703f, 0.1263362f, 0.4842942f, 0.2699945f
},
{ // coefficient G
0.02389163f, 0.02497956f, -0.1225606f, 0.4765765f, -0.07727878f, 0.1819978f
},
{ // coefficient H
2.774207f, 0.07090702f, 7.731542f, -2.025027f, 3.765006f, 2.239642f
},
{ // coefficient I
0.5242274f, 0.3252744f, 1.267577f, 0.1705113f, 0.9191738f, 0.6539653f
},
{ // radiance
0.01298035f, 0.0270911f, -0.0508457f, 0.2010066f, 0.3257828f, 0.2917322f
}
},
{ // turbidity 6
{ // coefficient A
-1.393024f, -1.831466f, -0.7994553f, -1.159543f, -1.044528f, -1.092518f
},
{ // coefficient B
-0.5200008f, -1.018106f, 0.1337239f, -0.283618f, -0.2152538f, -0.1865821f
},
{ // coefficient C
-19.41271f, 3.204932f, -1.563712f, 1.057464f, 1.023157f, 1.057856f
},
{ // coefficient D
19.86171f, -2.139369f, 0.7392042f, -0.3647533f, 0.05599579f, 0.3491427f
},
{ // coefficient E
-0.01212818f, -0.02441969f, -0.03136373f, -0.2513115f, 0.6321805f, -2.631526f
},
{ // coefficient F
0.5019138f, 0.3191029f, 1.008208f, -0.1995636f, 0.5155639f, 0.3584288f
},
{ // coefficient G
0.007897163f, 0.05563321f, -0.1988762f, 0.5943224f, -0.1111252f, 0.2191649f
},
{ // coefficient H
2.696459f, 0.132076f, 6.626579f, -0.4865436f, 2.28227f, 2.389704f
},
{ // coefficient I
0.5258316f, 0.326522f, 1.223213f, 0.2279895f, 0.8647371f, 0.6765946f
},
{ // radiance
0.01267531f, 0.02752851f, -0.0536445f, 0.1962715f, 0.3472458f, 0.2928354f
}
},
{ // turbidity 7
{ // coefficient A
-1.384989f, -2.08666f, -0.7309069f, -1.203529f, -1.002538f, -1.123572f
},
{ // coefficient B
-0.53057f, -1.234696f, 0.1413931f, -0.3216432f, -0.198448f, -0.2342682f
},
{ // coefficient C
-14.59855f, 2.053196f, -0.1773938f, -0.145897f, 1.233954f, 0.9679089f
},
{ // coefficient D
14.97673f, -1.257012f, 0.293882f, -0.5598789f, 0.8779171f, -0.001017991f
},
{ // coefficient E
-0.01471976f, -0.02129117f, -0.04933601f, -0.2934068f, 0.3133737f, -1.960819f
},
{ // coefficient F
0.4569028f, 0.1990309f, 1.496853f, -1.206192f, 1.311239f, -0.115368f
},
{ // coefficient G
0.005419006f, 0.08511032f, -0.3213358f, 0.7793654f, -0.2132532f, 0.2843114f
},
{ // coefficient H
2.73536f, -0.1312659f, 6.305852f, -0.922371f, 2.223702f, 1.779831f
},
{ // coefficient I
0.5389891f, 0.2800139f, 1.298086f, 0.122936f, 0.9461065f, 0.6400916f
},
{ // radiance
0.01200196f, 0.02782022f, -0.05449978f, 0.1818445f, 0.379542f, 0.2974612f
}
},
{ // turbidity 8
{ // coefficient A
-1.537393f, -1.817841f, -1.79053f, -0.3808658f, -1.37436f, -1.024167f
},
{ // coefficient B
-0.6924151f, -0.9662082f, -0.8332867f, 0.3525732f, -0.5517879f, -0.2097189f
},
{ // coefficient C
-12.39252f, 6.281905f, -2.605975f, 0.3897535f, 1.276626f, 0.8424991f
},
{ // coefficient D
12.85585f, -5.778709f, 3.383959f, -2.085715f, 1.821209f, 1.767689f
},
{ // coefficient E
-0.01666284f, -0.03131226f, -0.1685493f, -0.1845614f, -2.48088f, -1.312031f
},
{ // coefficient F
0.4429862f, 0.3074609f, 1.307568f, -1.470598f, 1.866494f, -0.8191386f
},
{ // coefficient G
0.00239823f, 0.04530074f, -0.2569465f, 0.7033938f, -0.1649246f, 0.2156968f
},
{ // coefficient H
2.210714f, 1.409838f, 3.272862f, 0.847513f, 0.6333864f, 1.201032f
},
{ // coefficient I
0.5509453f, 0.2538213f, 1.247743f, 0.1758974f, 0.9339444f, 0.6593457f
},
{ // radiance
0.01115769f, 0.02603247f, -0.04606362f, 0.1383509f, 0.4427481f, 0.2969281f
}
},
{ // turbidity 9
{ // coefficient A
-2.190652f, -1.972368f, -3.148093f, 0.5592882f, -1.706427f, -1.006197f
},
{ // coefficient B
-1.10658f, -0.8108244f, -2.033135f, 1.073057f, -0.8516236f, -0.1932306f
},
{ // coefficient C
-7.466636f, 4.233742f, -2.01965f, -0.02763735f, 1.014996f, 0.5337019f
},
{ // coefficient D
7.752518f, -3.927321f, 2.409779f, -1.563728f, 3.980336f, 1.797637f
},
{ // coefficient E
-0.02263662f, -0.05152609f, -0.01255187f, -1.191791f, -3.487485f, -0.9474717f
},
{ // coefficient F
0.4154878f, 0.2005285f, 0.9697956f, -0.4331798f, 0.6331985f, -0.1353274f
},
{ // coefficient G
-1.646586e-06f, 0.02985984f, -0.153808f, 0.4622361f, -0.0181838f, 0.2049954f
},
{ // coefficient H
2.076563f, 1.903602f, 2.617071f, 1.142664f, 0.735234f, 1.1561f
},
{ // coefficient I
0.6322611f, 0.1099985f, 1.234755f, 0.2988891f, 0.8452629f, 0.6894153f
},
{ // radiance
0.01034375f, 0.02111036f, -0.02502615f, 0.06215948f, 0.5253192f, 0.2999927f
}
},
{ // turbidity 10
{ // coefficient A
-2.473813f, -1.494464f, -6.305007f, 1.325532f, -1.590406f, -1.005785f
},
{ // coefficient B
-1.478132f, -0.7635918f, -3.323156f, 0.5765517f, -0.7529646f, -0.2286223f
},
{ // coefficient C
-3.49221f, 2.071212f, -0.7099379f, -0.7011257f, 1.930414f, 0.3330253f
},
{ // coefficient D
3.805303f, -1.536108f, 0.1686111f, 0.7720216f, 5.169169f, 1.438452f
},
{ // coefficient E
-0.03251673f, -0.1369345f, 0.1080881f, -2.01356f, -4.221403f, -0.7701349f
},
{ // coefficient F
0.3621052f, 0.1287259f, 0.8729732f, -0.1990572f, 0.04614858f, 0.02780764f
},
{ // coefficient G
-6.44777e-06f, 0.02147844f, -0.1291207f, 0.3922685f, -0.09527005f, 0.2630698f
},
{ // coefficient H
1.596383f, 1.75308f, 2.282989f, 0.009898769f, -0.7894472f, 0.8329894f
},
{ // coefficient I
0.855193f, -0.2246103f, 1.475588f, 0.178936f, 0.8458063f, 0.6747408f
},
{ // radiance
0.009270257f, 0.0125295f, 0.01085115f, -0.05004679f, 0.6169243f, 0.3053035f
}
}
}
},
{ // 520.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.171338f, -1.193029f, -1.165299f, -1.077288f, -1.07553f, -1.067002f
},
{ // coefficient B
-0.2379456f, -0.2445141f, -0.2503239f, -0.2659342f, -0.1232648f, -0.1572453f
},
{ // coefficient C
-6.515446f, -0.4311723f, 3.829843f, -0.07565678f, 1.877081f, 1.350156f
},
{ // coefficient D
7.133235f, 1.019354f, -2.234083f, 1.145086f, -0.1186427f, 0.003140948f
},
{ // coefficient E
-0.05382867f, -0.1281643f, -0.4693988f, -0.02316655f, -2.748628f, -3.277283f
},
{ // coefficient F
0.6889982f, 0.7837279f, 1.045109f, 1.564941f, 1.249377f, 1.488404f
},
{ // coefficient G
0.04250983f, 0.02609613f, -0.06241707f, 0.09375322f, -0.108578f, 0.1127058f
},
{ // coefficient H
4.471437f, 5.9658f, 0.262331f, -0.7476929f, 3.838331f, 2.65573f
},
{ // coefficient I
0.5087463f, 0.4381607f, 0.7500398f, 0.6076913f, 0.8601227f, 0.4999138f
},
{ // radiance
0.01459826f, 0.01539451f, -0.001848659f, 0.1563602f, 0.0677332f, 0.09272923f
}
},
{ // turbidity 2
{ // coefficient A
-1.175145f, -1.226543f, -1.075743f, -1.104008f, -1.080135f, -1.06628f
},
{ // coefficient B
-0.2454528f, -0.297995f, -0.1267529f, -0.2079352f, -0.1942481f, -0.1421591f
},
{ // coefficient C
-8.17328f, 3.102814f, 0.1017884f, 1.094572f, 1.409056f, 1.272936f
},
{ // coefficient D
8.622722f, -2.662701f, 0.4839556f, 1.147058f, -3.800384f, 7.891881f
},
{ // coefficient E
-0.03895019f, -0.1012309f, -0.4025119f, -0.1516726f, -7.125701f, -3.626975f
},
{ // coefficient F
0.6739096f, 0.5142095f, 1.120466f, 1.526097f, 0.973122f, 1.294678f
},
{ // coefficient G
0.06622835f, 0.04939276f, -0.05059103f, 0.1922554f, -0.07312793f, 0.1331475f
},
{ // coefficient H
4.450872f, 5.007851f, 3.60107f, 0.4060614f, 2.37781f, 2.986127f
},
{ // coefficient I
0.5061382f, 0.4179865f, 1.104232f, 0.2975787f, 0.9223131f, 0.5316962f
},
{ // radiance
0.01433599f, 0.01641988f, -0.01010719f, 0.1752844f, 0.07101618f, 0.0942392f
}
},
{ // turbidity 3
{ // coefficient A
-1.210057f, -1.311087f, -0.9705912f, -1.177697f, -1.02942f, -1.082084f
},
{ // coefficient B
-0.282764f, -0.3921139f, -0.04316197f, -0.1893604f, -0.2105096f, -0.1321893f
},
{ // coefficient C
-5.179186f, 1.436554f, 0.5357225f, -0.05207247f, 1.657176f, 0.8487219f
},
{ // coefficient D
5.51443f, -0.705126f, -1.399982f, 4.572666f, -10.28655f, 18.5829f
},
{ // coefficient E
-0.05638504f, -0.1462714f, -0.3407923f, -0.5523178f, -7.725428f, -3.172656f
},
{ // coefficient F
0.6656727f, 0.260462f, 1.585242f, 0.2677765f, 1.707122f, 0.6915801f
},
{ // coefficient G
0.05417868f, 0.0577157f, -0.06423039f, 0.3872768f, -0.07681108f, 0.1905927f
},
{ // coefficient H
4.21655f, 3.493766f, 5.338193f, 1.978123f, 1.07466f, 3.901473f
},
{ // coefficient I
0.5495231f, 0.5041164f, 0.9835385f, 0.4095974f, 0.8312794f, 0.6144141f
},
{ // radiance
0.01361165f, 0.02045058f, -0.03003054f, 0.2142268f, 0.06624853f, 0.1021088f
}
},
{ // turbidity 4
{ // coefficient A
-1.259854f, -1.41322f, -0.992684f, -1.113409f, -1.055595f, -1.070779f
},
{ // coefficient B
-0.332963f, -0.5430979f, 0.02303697f, -0.2409045f, -0.170368f, -0.1445161f
},
{ // coefficient C
-2.270176f, -0.1715627f, 0.7306051f, -0.1979662f, 1.089771f, 0.6385947f
},
{ // coefficient D
2.357599f, 1.151561f, -2.124879f, 5.089736f, -10.41463f, 16.95445f
},
{ // coefficient E
-0.09019414f, -0.2318489f, -0.2221853f, -2.07184f, -4.687545f, -3.724275f
},
{ // coefficient F
0.5779792f, 0.2791441f, 1.635982f, -0.6954891f, 2.149733f, -0.03465499f
},
{ // coefficient G
0.07706039f, 0.01306646f, -0.08684297f, 0.5966469f, -0.1118987f, 0.3645376f
},
{ // coefficient H
3.971905f, 1.794583f, 7.621636f, -1.346819f, 3.55881f, 3.23241f
},
{ // coefficient I
0.4999172f, 0.6167457f, 0.9443863f, 0.375208f, 0.8741214f, 0.6285788f
},
{ // radiance
0.01263245f, 0.02450953f, -0.05278634f, 0.255409f, 0.07442617f, 0.1093243f
}
},
{ // turbidity 5
{ // coefficient A
-1.297812f, -1.680599f, -0.8467684f, -1.182658f, -1.032316f, -1.07306f
},
{ // coefficient B
-0.3805039f, -0.8036751f, 0.1620743f, -0.3290773f, -0.1590425f, -0.1623303f
},
{ // coefficient C
-1.474455f, 0.8267015f, -0.5520307f, 0.1442907f, 0.9289968f, 0.3798836f
},
{ // coefficient D
1.558382f, -0.21092f, 0.8293798f, -1.705529f, 12.17091f, 2.223272f
},
{ // coefficient E
-0.1292485f, -0.4326451f, 0.8728132f, -11.86174f, 1.27469f, -4.660805f
},
{ // coefficient F
0.5206238f, 0.384393f, 0.9075147f, -0.190304f, 1.400296f, -0.1172357f
},
{ // coefficient G
0.07166147f, 0.05207142f, -0.1734859f, 0.7826049f, -0.3047612f, 0.5306455f
},
{ // coefficient H
3.65115f, 0.693852f, 7.815672f, -1.497409f, 2.805729f, 3.10552f
},
{ // coefficient I
0.4998781f, 0.4752346f, 1.126843f, 0.2238945f, 0.9198989f, 0.6420864f
},
{ // radiance
0.01161553f, 0.02837614f, -0.07218472f, 0.2860706f, 0.08559036f, 0.1183292f
}
},
{ // turbidity 6
{ // coefficient A
-1.374072f, -1.704606f, -0.9651511f, -1.067172f, -1.063218f, -1.070058f
},
{ // coefficient B
-0.4453408f, -0.8629255f, 0.1186522f, -0.3214506f, -0.1412572f, -0.1684172f
},
{ // coefficient C
-1.081601f, -0.390504f, 0.3421397f, -0.393466f, 0.8153795f, 0.2225147f
},
{ // coefficient D
1.104314f, 2.152742f, -4.626814f, 9.25463f, -14.67486f, 23.45795f
},
{ // coefficient E
-0.1730612f, -0.6716606f, 0.971677f, -3.599603f, -2.02017f, -4.342429f
},
{ // coefficient F
0.5074812f, 0.5638527f, 0.7579588f, -0.1624031f, 1.094685f, -0.04661816f
},
{ // coefficient G
0.05784588f, -0.005967161f, -0.01064476f, 0.5478446f, 0.07786265f, 0.3545794f
},
{ // coefficient H
3.48815f, 0.4886292f, 8.154392f, -2.781628f, 3.719129f, 3.051769f
},
{ // coefficient I
0.5037112f, 0.4385432f, 1.103676f, 0.2905848f, 0.8826899f, 0.6567686f
},
{ // radiance
0.01106384f, 0.03079241f, -0.08377247f, 0.3007502f, 0.09584072f, 0.1242989f
}
},
{ // turbidity 7
{ // coefficient A
-1.397244f, -1.943117f, -1.007469f, -0.9885823f, -1.105054f, -1.061783f
},
{ // coefficient B
-0.4874379f, -1.042293f, 0.0428356f, -0.2796329f, -0.1835801f, -0.1955111f
},
{ // coefficient C
-2.217129f, 1.936194f, -1.830583f, 0.9017585f, 0.2099263f, 0.3405765f
},
{ // coefficient D
2.41578f, -0.7807718f, -0.1613949f, 0.9500529f, 3.626436f, 16.82185f
},
{ // coefficient E
-0.103929f, -0.4753317f, 0.5103059f, -8.751757f, 0.4241807f, -4.868111f
},
{ // coefficient F
0.5444596f, 0.6351817f, 0.1830881f, 0.5198673f, 0.02861915f, 0.3591669f
},
{ // coefficient G
0.02296795f, 0.03357824f, -0.06111472f, 0.611191f, -0.03245808f, 0.351867f
},
{ // coefficient H
3.242776f, 0.322157f, 7.698471f, -3.165369f, 3.560209f, 2.163473f
},
{ // coefficient I
0.557592f, 0.227739f, 1.322151f, 0.1193651f, 0.9581069f, 0.6425305f
},
{ // radiance
0.01036501f, 0.03253539f, -0.0927468f, 0.3036297f, 0.124937f, 0.1307019f
}
},
{ // turbidity 8
{ // coefficient A
-1.452178f, -2.146318f, -1.465359f, -0.7135462f, -1.18803f, -1.038065f
},
{ // coefficient B
-0.5662818f, -1.176122f, -0.3055025f, -0.2351046f, -0.2177467f, -0.1989059f
},
{ // coefficient C
-1.492883f, 1.480664f, -1.747423f, 1.111445f, 0.08022451f, 0.1630845f
},
{ // coefficient D
1.742373f, 0.1846814f, -2.538335f, 2.259275f, 8.746107f, 12.61969f
},
{ // coefficient E
-0.1894966f, -0.9003192f, 0.6082904f, -7.85381f, -0.6824017f, -4.006594f
},
{ // coefficient F
0.6129524f, 0.2819986f, 0.6523133f, -0.3006083f, 0.5968181f, -0.3213161f
},
{ // coefficient G
2.370115e-06f, -0.006858291f, 0.1426957f, 0.3100486f, 0.07024883f, 0.3948313f
},
{ // coefficient H
3.04256f, 0.544794f, 6.921276f, -4.047366f, 3.164998f, 2.040063f
},
{ // coefficient I
0.7988979f, -0.2109028f, 1.637843f, -0.03732308f, 0.9909158f, 0.6364874f
},
{ // radiance
0.009469294f, 0.03287197f, -0.093295f, 0.2773078f, 0.1866053f, 0.1366498f
}
},
{ // turbidity 9
{ // coefficient A
-1.78998f, -1.46808f, -3.911974f, 0.5986629f, -1.581111f, -0.9931334f
},
{ // coefficient B
-0.845014f, -0.7426812f, -1.873182f, 0.416093f, -0.5414559f, -0.2191364f
},
{ // coefficient C
-2.591234f, 2.369441f, -2.428294f, 1.499968f, 0.03449756f, 0.1647201f
},
{ // coefficient D
2.769295f, -1.635443f, 1.019786f, -4.234635f, 20.95624f, 2.95446f
},
{ // coefficient E
-0.08338896f, -0.3560365f, 0.2178584f, -7.153272f, -1.717086f, -2.889774f
},
{ // coefficient F
0.5620741f, 0.2130482f, 0.9599962f, -0.8777277f, 0.6913385f, -0.4261816f
},
{ // coefficient G
-8.271253e-06f, -0.002124886f, 0.03763471f, 0.4276508f, -0.122738f, 0.4166838f
},
{ // coefficient H
2.531602f, 1.863478f, 4.121844f, -2.26873f, 1.401315f, 1.370273f
},
{ // coefficient I
0.9001159f, -0.3426455f, 1.708236f, -0.09278595f, 0.9723511f, 0.6561021f
},
{ // radiance
0.008599754f, 0.02901873f, -0.07532574f, 0.2032418f, 0.2837623f, 0.1499765f
}
},
{ // turbidity 10
{ // coefficient A
-2.36852f, -1.498984f, -6.258071f, 0.718347f, -1.422091f, -1.018798f
},
{ // coefficient B
-1.245557f, -0.9483364f, -2.616781f, -0.133271f, -0.5984032f, -0.2003211f
},
{ // coefficient C
-3.473878f, 2.970418f, -3.186408f, 1.495243f, 0.2297508f, 0.05344788f
},
{ // coefficient D
3.511922f, -2.003511f, -0.03570416f, 3.859652f, 1.72678f, 14.74717f
},
{ // coefficient E
-0.04076311f, -0.3005171f, 0.5972101f, -5.605361f, -0.6684454f, -4.025558f
},
{ // coefficient F
0.4228576f, 0.324232f, 0.5638927f, 0.3808435f, -1.024912f, 0.901015f
},
{ // coefficient G
-8.136079e-06f, 0.006724107f, -0.08227073f, 0.49981f, -0.1967199f, 0.3605184f
},
{ // coefficient H
2.268992f, 1.693927f, 3.805214f, -1.207866f, -0.3214571f, 1.504726f
},
{ // coefficient I
0.9001266f, -0.2648941f, 1.486701f, 0.1082917f, 0.89223f, 0.67203f
},
{ // radiance
0.007760808f, 0.01977285f, -0.03328752f, 0.07084295f, 0.4155148f, 0.17086f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.150179f, -1.184017f, -1.129855f, -1.09003f, -1.048192f, -1.081709f
},
{ // coefficient B
-0.223343f, -0.233006f, -0.2313999f, -0.2445577f, -0.104872f, -0.1496534f
},
{ // coefficient C
-6.725138f, 0.1903877f, 3.299978f, 0.4546659f, 2.079951f, 1.433416f
},
{ // coefficient D
7.601675f, 0.4425206f, -1.86183f, 0.984886f, -0.0751985f, -8.415186e-05f
},
{ // coefficient E
-0.06176639f, -0.117333f, -0.3626476f, -0.4044981f, -1.451911f, -3.147387f
},
{ // coefficient F
0.7963802f, 0.2459899f, 1.858194f, 0.353274f, 1.354243f, 0.06964893f
},
{ // coefficient G
0.02476816f, 0.06799957f, -0.09845844f, 0.1409977f, -0.1508252f, 0.13454f
},
{ // coefficient H
4.311811f, 5.710763f, 0.6523143f, -0.7724273f, 4.159135f, 3.372381f
},
{ // coefficient I
0.5070506f, 0.4639162f, 0.6278178f, 0.2901711f, 0.8685415f, 0.5369835f
},
{ // radiance
0.01597952f, 0.01790201f, -0.001839686f, 0.1470389f, 0.1698205f, 0.2119268f
}
},
{ // turbidity 2
{ // coefficient A
-1.163329f, -1.234281f, -1.015235f, -1.171497f, -1.010262f, -1.102719f
},
{ // coefficient B
-0.2341078f, -0.3038288f, -0.100389f, -0.2465867f, -0.133746f, -0.1647974f
},
{ // coefficient C
-8.084459f, 3.30412f, 0.2834337f, 1.219344f, 1.953577f, 1.510508f
},
{ // coefficient D
8.765664f, -2.565467f, 0.4230399f, 1.18412f, -3.761183f, 7.573412f
},
{ // coefficient E
-0.04227908f, -0.1080261f, -0.4060874f, -0.06451707f, -7.0893f, -3.431528f
},
{ // coefficient F
0.6476384f, 0.5183037f, 1.119918f, 1.087554f, 0.5963565f, 0.3953995f
},
{ // coefficient G
0.05575993f, 0.0263307f, -0.02357803f, 0.1687897f, -0.08297304f, 0.06793657f
},
{ // coefficient H
4.291594f, 4.459108f, 3.215664f, 0.4111776f, 2.741114f, 2.744717f
},
{ // coefficient I
0.5023286f, 0.4526356f, 1.089777f, 0.2471485f, 0.9771003f, 0.5018037f
},
{ // radiance
0.01553599f, 0.0196927f, -0.01173079f, 0.1677871f, 0.1714108f, 0.21733f
}
},
{ // turbidity 3
{ // coefficient A
-1.186728f, -1.302626f, -0.9615438f, -1.178731f, -1.018913f, -1.101603f
},
{ // coefficient B
-0.2648685f, -0.3904258f, -0.04842098f, -0.1824314f, -0.2016682f, -0.1521834f
},
{ // coefficient C
-7.540497f, 3.431819f, -0.4756207f, 0.8266664f, 2.132865f, 1.348807f
},
{ // coefficient D
8.152868f, -2.975541f, 0.9408081f, 1.001614f, -3.323938f, 5.640453f
},
{ // coefficient E
-0.0436304f, -0.0766602f, -0.3108222f, 0.3024332f, -8.751654f, -2.695953f
},
{ // coefficient F
0.6836829f, 0.02464561f, 1.708814f, 0.03291998f, 1.24078f, -0.07496627f
},
{ // coefficient G
0.03221451f, 0.1054528f, -0.1241215f, 0.4178949f, -0.1801214f, 0.1719606f
},
{ // coefficient H
4.013046f, 3.161765f, 4.416567f, 2.532397f, 0.6145479f, 3.175091f
},
{ // coefficient I
0.6004295f, 0.3544185f, 1.207369f, 0.2014902f, 0.9457932f, 0.5968939f
},
{ // radiance
0.01489601f, 0.02206336f, -0.02576017f, 0.194916f, 0.1781909f, 0.2236776f
}
},
{ // turbidity 4
{ // coefficient A
-1.229694f, -1.436439f, -0.887636f, -1.235363f, -0.9785264f, -1.117781f
},
{ // coefficient B
-0.3167126f, -0.5562909f, 0.05717281f, -0.277819f, -0.1458813f, -0.1699302f
},
{ // coefficient C
-3.548352f, 0.801007f, 0.4713905f, 0.1577638f, 1.996989f, 1.21448f
},
{ // coefficient D
3.846155f, 0.3290243f, -1.66897f, 3.982457f, -7.517409f, 11.41083f
},
{ // coefficient E
-0.05028022f, -0.1385772f, -0.3199243f, -0.02266885f, -6.602487f, -3.253445f
},
{ // coefficient F
0.5061677f, 0.04556269f, 2.050582f, -1.411004f, 2.387711f, -0.8156284f
},
{ // coefficient G
0.07498335f, 0.02571935f, -0.0778398f, 0.5570462f, -0.1996484f, 0.2248021f
},
{ // coefficient H
3.626295f, 1.623385f, 6.117601f, 0.1740426f, 2.112759f, 2.8605f
},
{ // coefficient I
0.4999424f, 0.522222f, 1.070445f, 0.2747952f, 0.9289256f, 0.6151694f
},
{ // radiance
0.01384558f, 0.02528945f, -0.04419483f, 0.2249469f, 0.1968802f, 0.2312108f
}
},
{ // turbidity 5
{ // coefficient A
-1.290523f, -1.614452f, -0.8377211f, -1.2266f, -0.9852235f, -1.118112f
},
{ // coefficient B
-0.3813116f, -0.7953309f, 0.2276878f, -0.4426177f, -0.05890324f, -0.2040213f
},
{ // coefficient C
-0.6728796f, 0.5977485f, -0.3232539f, 0.6370202f, 1.529937f, 1.203227f
},
{ // coefficient D
1.007366f, 1.056365f, -2.3903f, 3.846913f, 4.30122f, 7.252102f
},
{ // coefficient E
-0.206803f, -0.8402403f, 2.12707f, -11.28719f, -1.839775f, -4.515955f
},
{ // coefficient F
0.4422499f, 0.3713919f, 0.7169058f, 0.03667721f, 0.8993504f, -0.2398231f
},
{ // coefficient G
0.06016254f, 0.0316472f, -0.04032313f, 0.5000027f, -0.1040866f, 0.2375855f
},
{ // coefficient H
3.218056f, -0.252663f, 9.072383f, -4.192432f, 4.636528f, 1.92316f
},
{ // coefficient I
0.5068822f, 0.4072481f, 1.188019f, 0.2376054f, 0.846671f, 0.6645269f
},
{ // radiance
0.01299228f, 0.02767429f, -0.05793193f, 0.2409692f, 0.2215448f, 0.238485f
}
},
{ // turbidity 6
{ // coefficient A
-1.330626f, -1.711989f, -0.8465894f, -1.192114f, -1.011026f, -1.106228f
},
{ // coefficient B
-0.4272516f, -0.8644776f, 0.1652715f, -0.3830569f, -0.1061217f, -0.1927917f
},
{ // coefficient C
-1.317682f, 0.4057135f, -0.2532361f, 0.5124751f, 1.357854f, 1.179701f
},
{ // coefficient D
1.650847f, 0.9037139f, -2.422693f, 7.280034f, -14.96195f, 23.79834f
},
{ // coefficient E
-0.1192771f, -0.3100084f, 0.3144841f, -2.610477f, -2.180975f, -4.870211f
},
{ // coefficient F
0.4904191f, 0.06317697f, 1.839347f, -1.832768f, 2.484329f, -1.290713f
},
{ // coefficient G
0.04074827f, 0.1065625f, -0.2818162f, 0.9101904f, -0.3239225f, 0.2854422f
},
{ // coefficient H
3.015846f, 0.0922624f, 7.856667f, -3.349116f, 3.899425f, 2.078973f
},
{ // coefficient I
0.5271835f, 0.3022474f, 1.387977f, -0.07313079f, 1.179264f, 0.5128625f
},
{ // radiance
0.01245635f, 0.02874175f, -0.06384005f, 0.2429023f, 0.2428387f, 0.2418906f
}
},
{ // turbidity 7
{ // coefficient A
-1.342815f, -1.937297f, -0.9292675f, -1.05885f, -1.079472f, -1.093159f
},
{ // coefficient B
-0.4571984f, -1.078823f, 0.1125551f, -0.3495292f, -0.1399984f, -0.2150433f
},
{ // coefficient C
-1.803521f, 2.178495f, -1.708569f, 1.135259f, 1.058148f, 1.210946f
},
{ // coefficient D
2.229578f, -1.2519f, 1.83284f, -4.069374f, 13.92667f, 3.349246f
},
{ // coefficient E
-0.1196587f, -0.3577875f, 0.5150525f, -9.727412f, -0.6245343f, -4.030345f
},
{ // coefficient F
0.5694038f, 0.2553915f, 0.9287322f, -0.7670653f, 1.192083f, -0.5362971f
},
{ // coefficient G
0.01194687f, 0.08081142f, -0.174678f, 0.7550431f, -0.2578858f, 0.2886842f
},
{ // coefficient H
2.95011f, -0.4723152f, 7.793815f, -4.105064f, 4.030723f, 1.192375f
},
{ // coefficient I
0.5961719f, 0.1174551f, 1.498832f, -0.05149354f, 1.025069f, 0.6363049f
},
{ // radiance
0.01170787f, 0.03001101f, -0.06977107f, 0.2368382f, 0.2775809f, 0.2482129f
}
},
{ // turbidity 8
{ // coefficient A
-1.408172f, -2.171702f, -1.284657f, -0.8288598f, -1.146031f, -1.069599f
},
{ // coefficient B
-0.5491538f, -1.231851f, -0.1935945f, -0.3374917f, -0.1480097f, -0.2047532f
},
{ // coefficient C
-1.360219f, 1.8604f, -1.494185f, 1.029011f, 1.051391f, 0.9437765f
},
{ // coefficient D
1.73815f, 0.03613146f, -2.070689f, 1.347088f, 8.396865f, 11.89879f
},
{ // coefficient E
-0.1505117f, -0.7300078f, 0.297402f, -7.655126f, -1.40701f, -4.02675f
},
{ // coefficient F
0.6030014f, 0.1974371f, 0.8048383f, -0.8232335f, 1.240651f, -0.9372092f
},
{ // coefficient G
-6.866289e-06f, -0.005351202f, 0.1097715f, 0.3722934f, -0.03942068f, 0.2463501f
},
{ // coefficient H
2.656101f, -0.1443434f, 6.90796f, -4.847675f, 3.291458f, 1.782902f
},
{ // coefficient I
0.9001849f, -0.3779605f, 1.819367f, -0.1864913f, 1.060212f, 0.6235027f
},
{ // radiance
0.01081211f, 0.0285476f, -0.06360294f, 0.1948047f, 0.3501631f, 0.2490269f
}
},
{ // turbidity 9
{ // coefficient A
-1.793508f, -1.629402f, -3.443431f, 0.313815f, -1.449802f, -1.04942f
},
{ // coefficient B
-0.8986075f, -0.827341f, -1.686429f, 0.2426714f, -0.4017798f, -0.2614006f
},
{ // coefficient C
-2.346395f, 2.27093f, -1.515036f, 0.7717995f, 1.133688f, 0.9473363f
},
{ // coefficient D
2.775625f, -1.125114f, -0.6941862f, 2.75952f, 2.114337f, 15.31288f
},
{ // coefficient E
-0.07052213f, -0.3562854f, 0.4976814f, -7.216077f, 0.06227965f, -4.714989f
},
{ // coefficient F
0.5349215f, 0.1948044f, 1.144483f, -1.389093f, 1.378688f, -0.8992715f
},
{ // coefficient G
-7.457456e-06f, 0.001703882f, -0.04277285f, 0.5332838f, -0.1759313f, 0.2423509f
},
{ // coefficient H
1.901248f, 1.426305f, 3.662107f, -2.316313f, 0.9446579f, 0.9148027f
},
{ // coefficient I
0.9001358f, -0.3737234f, 1.819972f, -0.2636593f, 1.165017f, 0.5650972f
},
{ // radiance
0.009903541f, 0.02416012f, -0.04342521f, 0.1140965f, 0.4468771f, 0.2561579f
}
},
{ // turbidity 10
{ // coefficient A
-2.382332f, -1.333339f, -6.612658f, 1.251113f, -1.517768f, -1.037889f
},
{ // coefficient B
-1.29076f, -0.9355999f, -2.80923f, 0.05915517f, -0.5807447f, -0.206578f
},
{ // coefficient C
-3.316114f, 3.281313f, -2.744061f, 1.142846f, 1.179477f, 0.6479897f
},
{ // coefficient D
3.587059f, -2.263103f, 1.005261f, -0.2807371f, 12.52252f, 0.04786848f
},
{ // coefficient E
-0.03748522f, -0.2499164f, 0.4216309f, -5.335452f, -2.305973f, -2.348244f
},
{ // coefficient F
0.4468493f, 0.1591742f, 1.055067f, -0.6967012f, 0.2184822f, -0.0305349f
},
{ // coefficient G
-7.212198e-06f, 0.008117981f, -0.09021558f, 0.5129558f, -0.3092917f, 0.4059549f
},
{ // coefficient H
1.789208f, 1.345396f, 2.940833f, -0.5264483f, -1.524373f, 1.313102f
},
{ // coefficient I
0.9001267f, -0.2689053f, 1.49527f, 0.1137786f, 0.8724218f, 0.6713422f
},
{ // radiance
0.008793464f, 0.01518177f, -0.004699154f, -0.009107614f, 0.555455f, 0.2692918f
}
}
}
},
{ // 560.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.121223f, -1.172565f, -1.124108f, -1.079809f, -1.086667f, -1.071154f
},
{ // coefficient B
-0.1710187f, -0.1880874f, -0.3082835f, -0.09226814f, -0.2709889f, -0.127215f
},
{ // coefficient C
-13.83038f, 7.283594f, -0.2179926f, 1.465364f, 2.249152f, 1.644374f
},
{ // coefficient D
14.75343f, -5.774643f, 1.330167f, 1.348856f, -3.177505f, 4.326598f
},
{ // coefficient E
-0.03137953f, -0.09051333f, -0.4502254f, 0.4357794f, -7.13813f, -2.244682f
},
{ // coefficient F
1.035662f, 0.9108126f, 1.47014f, 1.771543f, 1.112553f, 1.611051f
},
{ // coefficient G
0.04060064f, -0.0156541f, 0.0004202392f, 0.008121383f, -0.009718434f, 0.007835546f
},
{ // coefficient H
5.222551f, 6.194404f, -4.724308f, 6.057395f, -2.392239f, 3.213282f
},
{ // coefficient I
0.5001051f, 0.4688599f, 1.134678f, 0.2462141f, 0.9352768f, 0.6580432f
},
{ // radiance
0.01518543f, 0.01176421f, 0.01736355f, 0.108564f, 0.04928107f, 0.06789277f
}
},
{ // turbidity 2
{ // coefficient A
-1.133883f, -1.116119f, -1.215552f, -0.9657094f, -1.152042f, -1.051862f
},
{ // coefficient B
-0.1835049f, -0.1457114f, -0.3391607f, 0.03858547f, -0.3155103f, -0.1310458f
},
{ // coefficient C
-12.32301f, 5.436576f, 0.4502377f, 0.3324554f, 2.163244f, 1.554942f
},
{ // coefficient D
13.07796f, -5.114276f, 1.281602f, 1.54174f, -5.230832f, 10.6351f
},
{ // coefficient E
-0.03760123f, -0.07178414f, -0.4095778f, 0.6029572f, -7.443608f, -3.558989f
},
{ // coefficient F
1.051162f, 0.2658168f, 2.374058f, 0.2346414f, 2.247834f, 0.6764257f
},
{ // coefficient G
0.03392954f, 0.1603756f, -0.198843f, 0.3902458f, -0.1632216f, 0.1428912f
},
{ // coefficient H
5.319376f, 7.619379f, -3.809377f, 7.26305f, -1.725315f, 2.541641f
},
{ // coefficient I
0.6501601f, 0.3112698f, 1.254846f, 0.1353991f, 1.034709f, 0.5000389f
},
{ // radiance
0.01482072f, 0.01335627f, 0.007685462f, 0.1336625f, 0.04842644f, 0.07083679f
}
},
{ // turbidity 3
{ // coefficient A
-1.156925f, -1.183783f, -1.143101f, -1.021102f, -1.101145f, -1.073863f
},
{ // coefficient B
-0.2104634f, -0.2187288f, -0.2644541f, 0.02478746f, -0.2731481f, -0.1705854f
},
{ // coefficient C
-17.9309f, 5.860654f, 0.9354577f, -1.367363f, 2.483816f, 1.169673f
},
{ // coefficient D
18.17524f, -5.64396f, 0.9256854f, -0.09011684f, 0.358178f, -0.002183705f
},
{ // coefficient E
-0.01225514f, -0.04453459f, -0.1074845f, -0.500173f, 0.4249615f, -5.007439f
},
{ // coefficient F
0.6518753f, 0.6724995f, 1.663131f, 0.04316685f, 2.284087f, 0.2318154f
},
{ // coefficient G
0.1085708f, 0.01698133f, 0.04998918f, 0.3952231f, -0.05836688f, 0.2738714f
},
{ // coefficient H
4.947239f, 6.742131f, -2.049721f, 8.084754f, -1.651434f, 2.187327f
},
{ // coefficient I
0.5635117f, 0.6353553f, 0.8150826f, 0.5380131f, 0.768543f, 0.6676882f
},
{ // radiance
0.01406436f, 0.01604888f, -0.009000273f, 0.1735923f, 0.04526955f, 0.07742648f
}
},
{ // turbidity 4
{ // coefficient A
-1.207581f, -1.278614f, -1.150333f, -1.002248f, -1.087962f, -1.072272f
},
{ // coefficient B
-0.2653224f, -0.3516334f, -0.2033123f, -0.03092969f, -0.2434172f, -0.1614715f
},
{ // coefficient C
-9.004497f, 2.489304f, 0.8150312f, -1.092273f, 1.817322f, 0.8337136f
},
{ // coefficient D
8.98572f, -1.840089f, -0.7926382f, 2.820671f, -6.031774f, 10.86866f
},
{ // coefficient E
-0.009024461f, -0.06293646f, -0.3230951f, 0.4504455f, -4.873903f, -3.373042f
},
{ // coefficient F
0.4273132f, 0.8426903f, 1.291679f, -0.5121048f, 2.094606f, 0.02260332f
},
{ // coefficient G
0.1378091f, -0.07266949f, 0.1822329f, 0.4229688f, 0.1184743f, 0.2622468f
},
{ // coefficient H
4.307648f, 4.434869f, 1.619133f, 4.037073f, 0.2293346f, 2.470766f
},
{ // coefficient I
0.5309421f, 0.7077739f, 0.7522432f, 0.5843543f, 0.728394f, 0.7078601f
},
{ // radiance
0.01296165f, 0.02074811f, -0.03672731f, 0.2310227f, 0.04272776f, 0.08807197f
}
},
{ // turbidity 5
{ // coefficient A
-1.26385f, -1.507623f, -1.041397f, -1.049861f, -1.054117f, -1.072517f
},
{ // coefficient B
-0.3278851f, -0.6465694f, 0.06326589f, -0.2836044f, -0.1206625f, -0.1574135f
},
{ // coefficient C
-5.3364f, -2.983753f, 3.860471f, -2.302537f, 1.764625f, 0.2214599f
},
{ // coefficient D
5.102832f, 4.212027f, -5.579882f, 6.724969f, -9.467716f, 13.31766f
},
{ // coefficient E
0.00207002f, -0.05868795f, -0.1730201f, 0.4373598f, -3.436987f, -3.915381f
},
{ // coefficient F
0.3605369f, 0.3852488f, 2.303753f, -2.422514f, 3.394707f, -1.229973f
},
{ // coefficient G
0.1633426f, -0.07222995f, 0.06102393f, 0.6952438f, -0.08149683f, 0.6267275f
},
{ // coefficient H
3.790121f, 1.042808f, 7.287763f, -2.6336f, 3.485918f, 2.923262f
},
{ // coefficient I
0.4999503f, 0.663212f, 0.8916057f, 0.4070954f, 0.8515716f, 0.6516824f
},
{ // radiance
0.01185836f, 0.0255792f, -0.06240734f, 0.2766326f, 0.04714655f, 0.09809189f
}
},
{ // turbidity 6
{ // coefficient A
-1.293503f, -1.757697f, -0.8814599f, -1.125663f, -1.044841f, -1.06736f
},
{ // coefficient B
-0.3676147f, -0.8202513f, 0.0982896f, -0.2625873f, -0.1712889f, -0.1543712f
},
{ // coefficient C
-13.23095f, 2.560133f, -0.2701428f, -0.2797592f, 0.7091708f, 0.3884866f
},
{ // coefficient D
13.01713f, -1.337734f, -1.298657f, 2.848739f, -4.275119f, 6.915851f
},
{ // coefficient E
-0.0003471894f, -0.03413795f, -0.06721423f, -0.03363778f, -0.756513f, -4.250734f
},
{ // coefficient F
0.3957986f, 0.5633043f, 1.581675f, -1.672768f, 2.483873f, -0.6525729f
},
{ // coefficient G
0.131748f, -0.04355758f, 0.04095024f, 0.7349952f, -0.07781321f, 0.6368377f
},
{ // coefficient H
3.616827f, 0.7324869f, 6.548913f, -1.708628f, 2.667776f, 2.671865f
},
{ // coefficient I
0.499851f, 0.5728008f, 0.9726866f, 0.3557813f, 0.8709126f, 0.6566986f
},
{ // radiance
0.01107469f, 0.02825097f, -0.07618392f, 0.2978619f, 0.05429803f, 0.1044545f
}
},
{ // turbidity 7
{ // coefficient A
-1.360356f, -2.054147f, -0.8953952f, -1.030251f, -1.095631f, -1.052935f
},
{ // coefficient B
-0.4336026f, -1.064745f, 0.1048085f, -0.2777709f, -0.1966849f, -0.1551958f
},
{ // coefficient C
-14.42967f, -0.7857491f, 1.481293f, -0.8361086f, 0.6300566f, 0.2334578f
},
{ // coefficient D
14.28579f, 2.214734f, -4.223012f, 5.611154f, -7.129591f, 10.40948f
},
{ // coefficient E
-0.003990325f, -0.03764113f, -0.0416416f, 0.186124f, -0.7351951f, -4.426381f
},
{ // coefficient F
0.4600126f, 0.8504761f, 0.5999023f, -0.5823197f, 1.40034f, -0.3477323f
},
{ // coefficient G
0.1023159f, -0.04222958f, 0.06310005f, 0.671621f, -0.006997516f, 0.625707f
},
{ // coefficient H
3.367805f, 0.1091377f, 7.156883f, -3.269717f, 2.879018f, 2.451148f
},
{ // coefficient I
0.499915f, 0.4732161f, 1.066682f, 0.2922029f, 0.8879337f, 0.6570737f
},
{ // radiance
0.01030435f, 0.03060206f, -0.08858708f, 0.3072427f, 0.08473378f, 0.1077059f
}
},
{ // turbidity 8
{ // coefficient A
-1.382656f, -2.230895f, -1.489234f, -0.6206234f, -1.226372f, -1.030598f
},
{ // coefficient B
-0.4830539f, -1.163446f, -0.3402035f, -0.1701007f, -0.2128299f, -0.1931668f
},
{ // coefficient C
-11.07403f, -10.01991f, 2.948409f, -0.07346357f, -0.1643093f, 0.3691403f
},
{ // coefficient D
11.27854f, 11.2534f, -8.098077f, 9.350033f, -9.382334f, 10.66393f
},
{ // coefficient E
-0.02723641f, -0.03233871f, 0.09078299f, 0.1548964f, -0.4857784f, -4.244968f
},
{ // coefficient F
0.8478005f, 0.3041577f, -0.3179835f, 1.313206f, -0.1374948f, -0.05872571f
},
{ // coefficient G
0.002907652f, 0.003870458f, 0.4730321f, -0.1479143f, 0.5877245f, 0.419314f
},
{ // coefficient H
3.290364f, 0.3463879f, 6.765707f, -5.119165f, 3.862307f, 1.461878f
},
{ // coefficient I
0.739895f, -0.08829082f, 1.440217f, 0.1854277f, 0.8700125f, 0.6699478f
},
{ // radiance
0.009197751f, 0.03269422f, -0.09785581f, 0.2997964f, 0.1328366f, 0.1192367f
}
},
{ // turbidity 9
{ // coefficient A
-1.606656f, -2.100494f, -3.228292f, 0.1278814f, -1.400278f, -1.017511f
},
{ // coefficient B
-0.7140209f, -1.013602f, -1.459508f, 0.04010913f, -0.2825658f, -0.2884028f
},
{ // coefficient C
-11.49565f, -16.38934f, 7.424519f, -2.185675f, 0.4168149f, 0.3544996f
},
{ // coefficient D
11.68427f, 17.29185f, -11.94272f, 11.64548f, -11.95909f, 15.35118f
},
{ // coefficient E
-0.02245577f, -0.01580924f, 0.04931054f, 0.02706306f, -0.01122968f, -5.009184f
},
{ // coefficient F
0.7718953f, 0.05038208f, 0.6493074f, 0.1632886f, 0.1863328f, 0.1511064f
},
{ // coefficient G
-8.349882e-06f, -0.01628735f, 0.3590561f, -0.01090111f, 0.4263033f, 0.2916578f
},
{ // coefficient H
2.654449f, 1.762584f, 4.068321f, -3.34258f, 2.270053f, 0.7889658f
},
{ // coefficient I
0.9001224f, -0.3121794f, 1.613039f, 0.01916992f, 0.9748449f, 0.629491f
},
{ // radiance
0.008191103f, 0.03038883f, -0.08623921f, 0.2362785f, 0.2259917f, 0.133136f
}
},
{ // turbidity 10
{ // coefficient A
-2.031136f, -1.270967f, -6.600872f, 0.7961323f, -1.459073f, -1.021565f
},
{ // coefficient B
-1.076357f, -0.7828844f, -2.689874f, -0.1989144f, -0.4964034f, -0.2553022f
},
{ // coefficient C
-8.660773f, -11.98232f, 6.453928f, -2.685238f, 1.060127f, 0.06367571f
},
{ // coefficient D
8.800317f, 12.62753f, -9.592747f, 10.3423f, -12.77712f, 20.28825f
},
{ // coefficient E
-0.02370842f, -0.01509989f, -0.0355117f, 0.2076702f, -0.9634553f, -4.623219f
},
{ // coefficient F
0.5736027f, 0.1663438f, 1.325662f, -0.769235f, 0.5610879f, -0.02400428f
},
{ // coefficient G
-8.441313e-06f, 0.0002440939f, -0.01894375f, 0.6184894f, -0.2034286f, 0.4251423f
},
{ // coefficient H
2.248669f, 2.06864f, 3.506163f, -1.969413f, 0.352591f, 0.9640897f
},
{ // coefficient I
0.900128f, -0.2521574f, 1.469724f, 0.03842922f, 1.003048f, 0.6080298f
},
{ // radiance
0.00727592f, 0.02128876f, -0.0446489f, 0.1022425f, 0.3640615f, 0.1521652f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.121224f, -1.172565f, -1.124109f, -1.07981f, -1.086668f, -1.071158f
},
{ // coefficient B
-0.1710171f, -0.1880857f, -0.3082816f, -0.09226583f, -0.2709863f, -0.1272123f
},
{ // coefficient C
-13.83038f, 7.283595f, -0.2179926f, 1.465364f, 2.249153f, 1.644374f
},
{ // coefficient D
14.75343f, -5.774643f, 1.330167f, 1.348856f, -3.177505f, 4.326598f
},
{ // coefficient E
-0.03137812f, -0.09051285f, -0.4502252f, 0.4357794f, -7.13813f, -2.244682f
},
{ // coefficient F
1.035662f, 0.9108124f, 1.47014f, 1.771543f, 1.112553f, 1.611051f
},
{ // coefficient G
0.04060006f, -0.01565627f, 0.0004158616f, 0.008115381f, -0.00972421f, 0.007831866f
},
{ // coefficient H
5.222551f, 6.194404f, -4.724308f, 6.057395f, -2.392239f, 3.213282f
},
{ // coefficient I
0.5001051f, 0.4688599f, 1.134678f, 0.246214f, 0.9352768f, 0.6580432f
},
{ // radiance
0.016236f, 0.01386631f, 0.01790676f, 0.0999942f, 0.1334051f, 0.1524062f
}
},
{ // turbidity 2
{ // coefficient A
-1.125449f, -1.101273f, -1.213425f, -0.9610579f, -1.119731f, -1.094324f
},
{ // coefficient B
-0.1733403f, -0.1422106f, -0.308501f, 0.009458093f, -0.2765693f, -0.1418743f
},
{ // coefficient C
-12.28038f, 5.484715f, 0.5550585f, 0.5293256f, 2.433951f, 1.836224f
},
{ // coefficient D
13.11695f, -5.089835f, 1.302798f, 1.553214f, -5.22981f, 10.63054f
},
{ // coefficient E
-0.03541409f, -0.06655928f, -0.3946488f, 0.6053752f, -7.449777f, -3.561209f
},
{ // coefficient F
1.005494f, 0.2076815f, 2.285152f, 0.1172267f, 2.123304f, 0.5916534f
},
{ // coefficient G
0.04147852f, 0.1633101f, -0.1925101f, 0.367469f, -0.2242917f, 0.09296571f
},
{ // coefficient H
5.316886f, 7.624535f, -3.790065f, 7.30535f, -1.669115f, 2.60006f
},
{ // coefficient I
0.6051699f, 0.3044941f, 1.263207f, 0.1509544f, 1.041008f, 0.4999316f
},
{ // radiance
0.01580346f, 0.01594123f, 0.005795411f, 0.1311774f, 0.125668f, 0.1627637f
}
},
{ // turbidity 3
{ // coefficient A
-1.155818f, -1.17146f, -1.114385f, -1.070647f, -1.057912f, -1.105636f
},
{ // coefficient B
-0.2162981f, -0.1866443f, -0.2963805f, 0.04194546f, -0.2733753f, -0.1815423f
},
{ // coefficient C
-25.60068f, 8.497645f, 0.05955009f, -0.5701942f, 3.080618f, 1.651373f
},
{ // coefficient D
26.36088f, -8.396229f, 2.320529f, -1.001278f, 0.7477366f, -0.002553941f
},
{ // coefficient E
-0.01382944f, -0.03345417f, -0.07649266f, -0.3389615f, 0.3484876f, -5.008441f
},
{ // coefficient F
0.8101983f, 0.2960022f, 2.27794f, -0.8559533f, 2.214994f, -0.6378055f
},
{ // coefficient G
0.0470921f, 0.07985279f, -0.05942479f, 0.5070784f, -0.2597625f, 0.2370503f
},
{ // coefficient H
4.301909f, 7.420544f, -4.040964f, 9.80556f, -2.842961f, 1.890656f
},
{ // coefficient I
0.6416958f, 0.5552152f, 0.9588558f, 0.3529747f, 0.9178917f, 0.6018255f
},
{ // radiance
0.01508292f, 0.01888592f, -0.01186753f, 0.1719109f, 0.1235706f, 0.1739866f
}
},
{ // turbidity 4
{ // coefficient A
-1.194221f, -1.308216f, -1.048459f, -1.105796f, -1.033856f, -1.125219f
},
{ // coefficient B
-0.2570801f, -0.3781794f, -0.161772f, -0.04826962f, -0.2507382f, -0.1879003f
},
{ // coefficient C
-8.919095f, 2.369107f, 1.015136f, -0.8419705f, 2.882633f, 1.474838f
},
{ // coefficient D
9.380192f, -1.977066f, -0.2989579f, 3.285981f, -8.250912f, 14.53266f
},
{ // coefficient E
-0.0265047f, -0.04079773f, -0.2764989f, 0.7975109f, -6.878129f, -3.27191f
},
{ // coefficient F
0.6233645f, 0.144396f, 2.42799f, -1.954456f, 2.948966f, -1.231579f
},
{ // coefficient G
0.06663964f, 0.08206719f, -0.1147612f, 0.7424696f, -0.2850302f, 0.2397524f
},
{ // coefficient H
4.027438f, 3.465933f, 1.522921f, 4.604902f, -0.8253849f, 1.654932f
},
{ // coefficient I
0.6125383f, 0.5442867f, 1.04141f, 0.2602059f, 0.9844934f, 0.5625696f
},
{ // radiance
0.0138543f, 0.02238574f, -0.03390048f, 0.2155912f, 0.1366535f, 0.1840316f
}
},
{ // turbidity 5
{ // coefficient A
-1.24664f, -1.477471f, -1.046136f, -1.03391f, -1.09453f, -1.095556f
},
{ // coefficient B
-0.3202274f, -0.5819223f, -0.06763679f, -0.1295544f, -0.2282012f, -0.1807112f
},
{ // coefficient C
-6.355032f, 1.329471f, 0.6725076f, -0.4551294f, 2.298267f, 1.406193f
},
{ // coefficient D
6.364143f, -0.2694229f, -1.036786f, 2.659205f, -5.184881f, 8.120385f
},
{ // coefficient E
0.0009028376f, -0.04575905f, -0.3192799f, 0.8887235f, -6.122309f, -3.058239f
},
{ // coefficient F
0.3146975f, 0.5087866f, 1.724205f, -1.733153f, 2.590799f, -1.12115f
},
{ // coefficient G
0.1550718f, -0.06857139f, 0.06468388f, 0.6540117f, -0.1618707f, 0.2933606f
},
{ // coefficient H
3.413749f, 1.810092f, 3.953855f, 0.8046523f, 1.116033f, 1.488339f
},
{ // coefficient I
0.4999065f, 0.6318693f, 0.9417528f, 0.3597443f, 0.8867666f, 0.6374827f
},
{ // radiance
0.01289448f, 0.02572803f, -0.05406854f, 0.248165f, 0.1539663f, 0.1943329f
}
},
{ // turbidity 6
{ // coefficient A
-1.270123f, -1.663376f, -0.9518772f, -1.087154f, -1.064826f, -1.099622f
},
{ // coefficient B
-0.3552595f, -0.784684f, 0.07764573f, -0.2665646f, -0.1599892f, -0.1849797f
},
{ // coefficient C
-8.975893f, 0.6951405f, 1.247094f, -0.8624251f, 2.285337f, 1.193609f
},
{ // coefficient D
9.11008f, 0.3139107f, -2.224855f, 4.53606f, -8.584887f, 14.16481f
},
{ // coefficient E
-0.003388207f, -0.02448004f, -0.119531f, 0.418065f, -3.07456f, -5.009527f
},
{ // coefficient F
0.3559109f, 0.3572505f, 1.709581f, -1.629798f, 2.139459f, -0.8592493f
},
{ // coefficient G
0.1201097f, 0.01453572f, -0.08836329f, 0.847956f, -0.2592372f, 0.3485606f
},
{ // coefficient H
3.109904f, 0.6307761f, 5.688916f, -1.293696f, 1.937577f, 1.884203f
},
{ // coefficient I
0.4998957f, 0.5142914f, 1.113629f, 0.1869693f, 0.9957954f, 0.6097274f
},
{ // radiance
0.01229427f, 0.02715903f, -0.06212273f, 0.2543339f, 0.1780617f, 0.1961306f
}
},
{ // turbidity 7
{ // coefficient A
-1.344841f, -1.975928f, -0.8651864f, -1.10378f, -1.055963f, -1.111541f
},
{ // coefficient B
-0.4339942f, -1.04204f, 0.1288722f, -0.3279618f, -0.1527328f, -0.2123518f
},
{ // coefficient C
-9.193112f, 0.9695714f, 0.6811456f, -0.4193802f, 1.786114f, 1.286159f
},
{ // coefficient D
9.385864f, 0.3718f, -3.282787f, 7.331165f, -13.67578f, 22.97563f
},
{ // coefficient E
-0.009742818f, -0.03334078f, -0.09074685f, 0.6529659f, -3.450729f, -4.591526f
},
{ // coefficient F
0.481204f, 0.133362f, 1.894059f, -2.227297f, 2.641691f, -1.374672f
},
{ // coefficient G
0.07690647f, 0.08180282f, -0.1849744f, 0.9625482f, -0.3229222f, 0.3462811f
},
{ // coefficient H
2.843801f, -0.1652429f, 6.610614f, -3.348971f, 2.800131f, 1.071127f
},
{ // coefficient I
0.5228047f, 0.326701f, 1.322978f, -0.007261722f, 1.129409f, 0.5498514f
},
{ // radiance
0.01142362f, 0.02928317f, -0.07246911f, 0.2573202f, 0.2123595f, 0.2030761f
}
},
{ // turbidity 8
{ // coefficient A
-1.387544f, -2.206536f, -1.358286f, -0.7167767f, -1.210525f, -1.061524f
},
{ // coefficient B
-0.4964558f, -1.191342f, -0.226874f, -0.2969978f, -0.1245541f, -0.23235f
},
{ // coefficient C
-9.744778f, -2.244384f, 0.9805731f, 0.2300987f, 0.9742409f, 1.28488f
},
{ // coefficient D
10.14533f, 3.629413f, -5.163195f, 7.956088f, -9.705683f, 12.94836f
},
{ // coefficient E
-0.02463089f, -0.0360161f, -0.008589f, 0.6339728f, -1.658899f, -5.011732f
},
{ // coefficient F
0.7188705f, 0.07725349f, 0.7697565f, -0.6195374f, 1.365829f, -0.9141086f
},
{ // coefficient G
0.003919951f, 0.05357588f, 0.219875f, 0.2502844f, 0.20035f, 0.2723804f
},
{ // coefficient H
2.760792f, -0.2692147f, 7.029301f, -6.307569f, 4.400174f, 0.6225528f
},
{ // coefficient I
0.7369418f, -0.1142476f, 1.56717f, -0.02208822f, 1.042345f, 0.6030977f
},
{ // radiance
0.0103085f, 0.0297946f, -0.0756837f, 0.2359473f, 0.2693691f, 0.2158786f
}
},
{ // turbidity 9
{ // coefficient A
-1.548946f, -1.867128f, -3.470633f, 0.4095837f, -1.540633f, -1.01333f
},
{ // coefficient B
-0.6802676f, -0.967021f, -1.555121f, 0.1440005f, -0.3264234f, -0.2663837f
},
{ // coefficient C
-12.74231f, -6.847422f, 2.684966f, -0.560219f, 1.072405f, 1.168934f
},
{ // coefficient D
13.12954f, 7.952765f, -5.964736f, 7.110869f, -7.205546f, 9.315778f
},
{ // coefficient E
-0.01834225f, -0.01985986f, -0.005524158f, 0.2313366f, -0.5125251f, -4.320502f
},
{ // coefficient F
0.705158f, 0.01690548f, 1.187089f, -1.373206f, 2.009905f, -1.303393f
},
{ // coefficient G
-8.330446e-06f, -0.01176902f, 0.2552534f, 0.1973962f, 0.1531985f, 0.2825872f
},
{ // coefficient H
2.320435f, 0.8988469f, 4.394076f, -4.112998f, 2.113439f, 0.2365015f
},
{ // coefficient I
0.9001244f, -0.3242269f, 1.656399f, -0.05877027f, 1.042547f, 0.5993344f
},
{ // radiance
0.009256414f, 0.02622384f, -0.0591099f, 0.160285f, 0.3679614f, 0.2281095f
}
},
{ // turbidity 10
{ // coefficient A
-2.055426f, -1.643864f, -6.156336f, 0.7378485f, -1.34194f, -1.086126f
},
{ // coefficient B
-1.093229f, -0.9676934f, -2.568735f, -0.2614544f, -0.3847183f, -0.2811402f
},
{ // coefficient C
-3.487182f, -11.47133f, 6.499533f, -2.861777f, 2.045437f, 0.7554239f
},
{ // coefficient D
3.753855f, 12.5388f, -9.136753f, 9.518947f, -11.79073f, 18.62181f
},
{ // coefficient E
-0.04639837f, 0.02347351f, -0.1751599f, 0.5033808f, -1.532773f, -4.343385f
},
{ // coefficient F
0.5797714f, -0.01602964f, 1.87696f, -1.844722f, 1.639939f, -0.8611011f
},
{ // coefficient G
-8.056992e-06f, 0.004481579f, -0.07959051f, 0.6855962f, -0.2668175f, 0.3146558f
},
{ // coefficient H
1.916262f, 1.373645f, 3.074951f, -1.497263f, -0.8401827f, 0.7022438f
},
{ // coefficient I
0.9001298f, -0.2884929f, 1.579874f, -0.09081643f, 1.083722f, 0.5872694f
},
{ // radiance
0.008220083f, 0.01701079f, -0.01768254f, 0.02593519f, 0.4977242f, 0.2394514f
}
}
}
},
{ // 600.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.120756f, -1.170564f, -1.036563f, -1.193752f, -0.9950781f, -1.10682f
},
{ // coefficient B
-0.175605f, -0.1845108f, -0.2605284f, -0.1236757f, -0.2103974f, -0.1804412f
},
{ // coefficient C
-3.557732f, 1.081952f, 3.803752f, -0.2036484f, 2.992004f, 1.960315f
},
{ // coefficient D
5.117996f, 1.330153f, -3.5854f, 4.137855f, -4.050491f, 3.667064f
},
{ // coefficient E
-0.1042966f, -0.2486698f, -0.9070196f, 1.566414f, -6.354208f, -2.460232f
},
{ // coefficient F
1.269364f, 0.7864551f, 2.380958f, 0.8510547f, 2.250652f, 0.9553453f
},
{ // coefficient G
0.01318863f, 0.003710973f, -0.0132068f, 0.01653213f, -0.01548511f, 0.01238019f
},
{ // coefficient H
3.718263f, 4.567181f, -4.210738f, 5.488784f, -1.876535f, 1.307691f
},
{ // coefficient I
0.5393663f, 0.4998408f, 1.085121f, 0.233591f, 0.9733518f, 0.556471f
},
{ // radiance
0.01605147f, 0.01028116f, 0.02949675f, 0.07265851f, 0.04608279f, 0.05069475f
}
},
{ // turbidity 2
{ // coefficient A
-1.112834f, -1.16091f, -1.054872f, -1.147542f, -1.023945f, -1.100815f
},
{ // coefficient B
-0.1534956f, -0.1640745f, -0.2642844f, 0.01011446f, -0.2867193f, -0.1590204f
},
{ // coefficient C
-4.968015f, 0.7416807f, 4.375802f, -2.308529f, 3.73366f, 1.41333f
},
{ // coefficient D
6.079068f, -0.2894668f, -1.434824f, 1.587994f, -2.572157f, 6.546557f
},
{ // coefficient E
-0.08917942f, -0.08983936f, -0.7521504f, 1.226957f, -7.749675f, -2.407235f
},
{ // coefficient F
1.167545f, 0.03778575f, 3.669027f, -1.328837f, 3.927326f, -0.2678587f
},
{ // coefficient G
0.05945633f, 0.21333f, -0.2992879f, 0.5996235f, -0.3230666f, 0.2367155f
},
{ // coefficient H
5.295468f, 6.390765f, -5.159653f, 10.04278f, -4.260418f, 2.242596f
},
{ // coefficient I
0.6272123f, 0.3592393f, 1.284643f, 0.05212352f, 1.091761f, 0.5273419f
},
{ // radiance
0.01569474f, 0.01190744f, 0.01996322f, 0.1044303f, 0.0374612f, 0.05764085f
}
},
{ // turbidity 3
{ // coefficient A
-1.137271f, -1.136415f, -1.173214f, -1.021346f, -1.082107f, -1.076296f
},
{ // coefficient B
-0.1838447f, -0.1536696f, -0.3202233f, 0.07262425f, -0.2721172f, -0.1655466f
},
{ // coefficient C
-2.268731f, -1.821062f, 4.868221f, -2.940205f, 3.071962f, 1.323765f
},
{ // coefficient D
3.013967f, 1.756804f, -2.485467f, 3.357462f, -7.058088f, 14.96892f
},
{ // coefficient E
-0.1471213f, -0.09313635f, -0.7693969f, 1.150486f, -5.103538f, -3.773928f
},
{ // coefficient F
0.9047947f, 0.2208938f, 3.027854f, -1.710207f, 3.852302f, -0.5211752f
},
{ // coefficient G
0.08086373f, 0.2021774f, -0.1745243f, 0.7381637f, -0.2281911f, 0.2583009f
},
{ // coefficient H
4.834215f, 7.289664f, -4.896365f, 9.857427f, -2.565665f, 1.797906f
},
{ // coefficient I
0.6545047f, 0.4997813f, 1.014186f, 0.3036725f, 0.9669584f, 0.5888323f
},
{ // radiance
0.01507252f, 0.01350309f, 0.007291417f, 0.1433035f, 0.03625213f, 0.06291384f
}
},
{ // turbidity 4
{ // coefficient A
-1.180432f, -1.260549f, -1.119712f, -1.056386f, -1.044811f, -1.090027f
},
{ // coefficient B
-0.2327942f, -0.2865438f, -0.252116f, 0.02018258f, -0.2531291f, -0.1699462f
},
{ // coefficient C
-0.715265f, -2.73677f, 4.860527f, -3.728955f, 3.102204f, 0.7118896f
},
{ // coefficient D
1.07812f, 2.720969f, -1.950603f, 1.02227f, -3.662632f, 12.87572f
},
{ // coefficient E
-0.2935788f, 0.1291231f, -1.598805f, 1.72221f, -5.609358f, -2.747429f
},
{ // coefficient F
0.6977295f, 0.4072562f, 2.556718f, -2.691554f, 4.574385f, -1.706223f
},
{ // coefficient G
0.111853f, 0.04510932f, 0.02462723f, 0.8329883f, -0.21736f, 0.4483382f
},
{ // coefficient H
4.270574f, 5.412464f, -2.059404f, 6.98245f, -1.64611f, 2.029605f
},
{ // coefficient I
0.6241267f, 0.6781695f, 0.8000995f, 0.4746778f, 0.8593305f, 0.6366599f
},
{ // radiance
0.01383183f, 0.01670171f, -0.01576321f, 0.1993796f, 0.03837478f, 0.07063028f
}
},
{ // turbidity 5
{ // coefficient A
-1.232266f, -1.451205f, -1.059832f, -1.040966f, -1.061923f, -1.070607f
},
{ // coefficient B
-0.2905676f, -0.5169964f, -0.07658003f, -0.1269405f, -0.1981907f, -0.1627049f
},
{ // coefficient C
-0.4158347f, -0.2018331f, 0.6456894f, -0.3751056f, 1.089854f, 0.7012071f
},
{ // coefficient D
0.5285264f, 1.499655f, -4.398594f, 12.29395f, -3.414507f, 2.13878f
},
{ // coefficient E
-0.3064312f, -1.575963f, 5.369604f, -19.7709f, 8.032592f, -5.006595f
},
{ // coefficient F
0.5569478f, 0.2761459f, 2.121235f, -2.458488f, 3.459965f, -1.096698f
},
{ // coefficient G
0.1195289f, 0.05184923f, 0.03210869f, 0.7711932f, -0.1253984f, 0.6658771f
},
{ // coefficient H
3.788091f, 2.301119f, 3.542794f, 0.8079227f, 1.188198f, 1.99943f
},
{ // coefficient I
0.597068f, 0.6177771f, 0.9114953f, 0.3865305f, 0.8639277f, 0.6557122f
},
{ // radiance
0.01253723f, 0.0226152f, -0.04847485f, 0.2646653f, 0.02855366f, 0.08568437f
}
},
{ // turbidity 6
{ // coefficient A
-1.28009f, -1.64819f, -0.9589189f, -1.076589f, -1.049437f, -1.069986f
},
{ // coefficient B
-0.3357179f, -0.7024227f, 0.02559557f, -0.1936109f, -0.1694738f, -0.1830827f
},
{ // coefficient C
-0.09337154f, -0.02925797f, 0.2192186f, -0.4162615f, 1.06211f, 0.5006422f
},
{ // coefficient D
-0.005075553f, 1.658094f, -5.276316f, 12.02609f, 5.575299f, -0.003063835f
},
{ // coefficient E
-3.096112f, 4.229924f, -4.153922f, -9.327314f, 0.4846701f, -2.768514f
},
{ // coefficient F
0.4001499f, 0.3502683f, 1.855574f, -2.326508f, 3.089474f, -1.258027f
},
{ // coefficient G
0.1489818f, 0.03180108f, 0.03052317f, 0.7983253f, -0.1584214f, 0.6679955f
},
{ // coefficient H
3.515349f, 1.430955f, 4.323694f, 0.2100845f, 1.369457f, 1.840288f
},
{ // coefficient I
0.5571477f, 0.5739552f, 0.9839497f, 0.313517f, 0.8995377f, 0.6498676f
},
{ // radiance
0.01179552f, 0.02546904f, -0.0650315f, 0.2924065f, 0.03516626f, 0.09101182f
}
},
{ // turbidity 7
{ // coefficient A
-1.328971f, -2.023062f, -0.8575747f, -1.073428f, -1.05712f, -1.060079f
},
{ // coefficient B
-0.385323f, -1.02384f, 0.1580212f, -0.3228761f, -0.1266841f, -0.181668f
},
{ // coefficient C
-0.2343098f, 0.4308501f, -0.7365004f, 0.4793085f, 0.4555368f, 0.3541107f
},
{ // coefficient D
-0.006505414f, 1.473146f, -4.007773f, 5.455778f, 12.01278f, -0.002642196f
},
{ // coefficient E
-4.859294f, 6.112105f, -4.051657f, -7.830559f, -0.9293773f, -2.588317f
},
{ // coefficient F
0.3752993f, 0.5767265f, 0.8889282f, -0.9794478f, 1.439595f, -0.564001f
},
{ // coefficient G
0.1670718f, -0.03107377f, 0.1978413f, 0.5370543f, 0.03262182f, 0.6643994f
},
{ // coefficient H
3.474961f, 0.07748759f, 6.282132f, -3.02275f, 2.890364f, 1.673077f
},
{ // coefficient I
0.4998573f, 0.5016978f, 1.063928f, 0.28186f, 0.882154f, 0.6658333f
},
{ // radiance
0.01070809f, 0.03005971f, -0.08793572f, 0.3255182f, 0.04780714f, 0.1014896f
}
},
{ // turbidity 8
{ // coefficient A
-1.395409f, -2.418167f, -1.221212f, -0.7182331f, -1.207117f, -1.015927f
},
{ // coefficient B
-0.4658947f, -1.266061f, -0.1072571f, -0.2519055f, -0.1635739f, -0.1890895f
},
{ // coefficient C
-0.4370597f, 0.3287382f, -1.655914f, 1.641261f, -0.3256308f, 0.4180376f
},
{ // coefficient D
1.342039f, -1.486721f, 2.659047f, -5.500375f, 14.09033f, 9.528535f
},
{ // coefficient E
-1.108674f, -0.677018f, 6.83077f, -12.53143f, 1.107883f, -4.073979f
},
{ // coefficient F
0.6439316f, 0.2439008f, 0.4984792f, -0.1542672f, 0.7217342f, -0.8713087f
},
{ // coefficient G
0.01355229f, 0.1703788f, 0.2538158f, 0.1741161f, 0.315886f, 0.4899379f
},
{ // coefficient H
3.241153f, 0.01664559f, 6.50066f, -4.907497f, 3.53755f, 1.277831f
},
{ // coefficient I
0.6696079f, -0.006330217f, 1.486576f, 0.07968864f, 0.9389751f, 0.6519818f
},
{ // radiance
0.009466972f, 0.03330913f, -0.1028545f, 0.3284098f, 0.09602966f, 0.1103224f
}
},
{ // turbidity 9
{ // coefficient A
-1.551242f, -1.971409f, -3.464015f, 0.4069978f, -1.504217f, -0.9753124f
},
{ // coefficient B
-0.6400568f, -0.9004995f, -1.604654f, 0.2775394f, -0.3618442f, -0.2080868f
},
{ // coefficient C
-0.3673011f, -0.2337582f, -1.514516f, 1.525761f, -0.2305374f, 0.2450141f
},
{ // coefficient D
1.623228f, -1.511905f, 2.768075f, -5.264056f, 14.34481f, -0.003535206f
},
{ // coefficient E
-1.482572f, 0.06645432f, 4.084166f, -7.108445f, -1.713254f, -2.121317f
},
{ // coefficient F
0.5625944f, 0.6149087f, -0.4287171f, 1.134996f, -0.5670708f, -0.1425634f
},
{ // coefficient G
-1.130924e-05f, -0.03184094f, 0.7112107f, -0.4122625f, 0.5206848f, 0.4670084f
},
{ // coefficient H
2.792007f, 2.02621f, 2.89646f, -2.558151f, 1.704009f, 1.027875f
},
{ // coefficient I
0.9000924f, -0.2799058f, 1.502031f, 0.2009854f, 0.8447497f, 0.6735543f
},
{ // radiance
0.008190696f, 0.03248675f, -0.09882934f, 0.2798409f, 0.1829752f, 0.1263322f
}
},
{ // turbidity 10
{ // coefficient A
-2.027207f, -1.506235f, -6.482777f, 0.9357272f, -1.398229f, -1.070734f
},
{ // coefficient B
-1.0036f, -0.7795241f, -2.700075f, -0.1066566f, -0.3167573f, -0.3636167f
},
{ // coefficient C
-0.3813114f, -0.4313309f, -1.556622f, 1.476763f, -0.1276866f, 0.242649f
},
{ // coefficient D
0.9357907f, 0.9124054f, -2.978497f, 6.889797f, 1.23944f, 8.282765f
},
{ // coefficient E
-1.031886f, -0.7484352f, 4.623255f, -10.38036f, 1.816678f, -3.58871f
},
{ // coefficient F
0.5523389f, -0.08417756f, 1.953868f, -2.094856f, 1.586775f, -0.9105402f
},
{ // coefficient G
-8.88394e-06f, -0.01426561f, 0.3113178f, 0.1330069f, 0.08043533f, 0.3760276f
},
{ // coefficient H
2.339379f, 2.571178f, 2.311303f, -1.446404f, 0.3219192f, 0.5756358f
},
{ // coefficient I
0.9001659f, -0.2504298f, 1.451021f, 0.112189f, 0.920062f, 0.6508161f
},
{ // radiance
0.007225549f, 0.02433471f, -0.06029571f, 0.1468935f, 0.3282492f, 0.1441367f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.120757f, -1.170565f, -1.036564f, -1.193753f, -0.9950796f, -1.106823f
},
{ // coefficient B
-0.1756034f, -0.1845093f, -0.2605265f, -0.1236733f, -0.2103948f, -0.1804388f
},
{ // coefficient C
-3.557732f, 1.081952f, 3.803752f, -0.2036483f, 2.992004f, 1.960315f
},
{ // coefficient D
5.117996f, 1.330153f, -3.5854f, 4.137855f, -4.050491f, 3.667064f
},
{ // coefficient E
-0.104296f, -0.2486697f, -0.9070196f, 1.566414f, -6.354208f, -2.460232f
},
{ // coefficient F
1.269364f, 0.786455f, 2.380958f, 0.8510543f, 2.250652f, 0.9553451f
},
{ // coefficient G
0.01318834f, 0.003709405f, -0.01320996f, 0.01652749f, -0.01548996f, 0.01237721f
},
{ // coefficient H
3.718263f, 4.567181f, -4.210738f, 5.488784f, -1.876535f, 1.307692f
},
{ // coefficient I
0.5393663f, 0.4998408f, 1.085121f, 0.233591f, 0.9733518f, 0.556471f
},
{ // radiance
0.01676204f, 0.0126948f, 0.02674342f, 0.0738077f, 0.1072658f, 0.1211512f
}
},
{ // turbidity 2
{ // coefficient A
-1.113356f, -1.134232f, -1.10504f, -1.078302f, -1.070883f, -1.09088f
},
{ // coefficient B
-0.1560185f, -0.1667475f, -0.2389195f, -0.02258635f, -0.2752054f, -0.1453755f
},
{ // coefficient C
-7.788803f, 4.176259f, 0.986626f, 0.7407482f, 2.937203f, 2.046382f
},
{ // coefficient D
8.963658f, -3.242194f, 1.271557f, 0.1805864f, -2.246366f, 5.595795f
},
{ // coefficient E
-0.05322872f, -0.0788923f, -0.6066849f, 0.907058f, -9.847865f, -2.809181f
},
{ // coefficient F
1.11667f, 0.08254326f, 3.5999f, -1.72082f, 3.104714f, -0.9239576f
},
{ // coefficient G
0.07024842f, 0.1400333f, -0.1681445f, 0.4256187f, -0.2573859f, 0.1783176f
},
{ // coefficient H
4.886694f, 5.395289f, -2.812867f, 6.982502f, -2.816401f, 2.05135f
},
{ // coefficient I
0.5931909f, 0.4862375f, 1.053791f, 0.3350778f, 0.84789f, 0.5815062f
},
{ // radiance
0.01649305f, 0.01445974f, 0.01662432f, 0.1056364f, 0.1002831f, 0.131733f
}
},
{ // turbidity 3
{ // coefficient A
-1.136641f, -1.146478f, -1.136317f, -1.044973f, -1.076125f, -1.100551f
},
{ // coefficient B
-0.1825277f, -0.1593833f, -0.3103198f, 0.07826651f, -0.2930526f, -0.1533449f
},
{ // coefficient C
-3.464338f, -0.905579f, 4.219694f, -2.043684f, 3.722784f, 1.767589f
},
{ // coefficient D
4.413974f, 0.8991902f, -2.031887f, 3.41072f, -7.364061f, 14.79022f
},
{ // coefficient E
-0.1123917f, -0.04711416f, -0.7184952f, 1.974969f, -7.399428f, -2.910058f
},
{ // coefficient F
1.009115f, -0.3813573f, 3.795646f, -2.736513f, 4.034307f, -1.732923f
},
{ // coefficient G
0.05542918f, 0.2290428f, -0.1650547f, 0.6828176f, -0.3489831f, 0.2193598f
},
{ // coefficient H
4.702599f, 6.961903f, -5.402471f, 10.49084f, -3.438955f, 1.880974f
},
{ // coefficient I
0.6799974f, 0.4206011f, 1.118561f, 0.1894241f, 1.077145f, 0.4999331f
},
{ // radiance
0.01577677f, 0.01637005f, 0.002821156f, 0.1465189f, 0.09971575f, 0.1424798f
}
},
{ // turbidity 4
{ // coefficient A
-1.175282f, -1.224868f, -1.144647f, -1.028428f, -1.07673f, -1.107957f
},
{ // coefficient B
-0.2274651f, -0.2528005f, -0.3063749f, 0.08888974f, -0.312926f, -0.1728652f
},
{ // coefficient C
-0.8788053f, -2.861303f, 4.347579f, -2.431225f, 3.58256f, 1.67954f
},
{ // coefficient D
1.446934f, 3.033667f, -2.473291f, 2.811511f, -5.902417f, 12.38391f
},
{ // coefficient E
-0.2568124f, 0.1405049f, -1.180139f, 2.400771f, -6.959342f, -2.94562f
},
{ // coefficient F
0.7201567f, 0.2481768f, 2.550631f, -2.380403f, 3.52803f, -1.568811f
},
{ // coefficient G
0.1054963f, 0.06435189f, 0.03833967f, 0.7114868f, -0.2613596f, 0.2418421f
},
{ // coefficient H
4.149007f, 5.409907f, -3.231444f, 8.190106f, -2.993803f, 1.360652f
},
{ // coefficient I
0.608406f, 0.6372238f, 0.8737219f, 0.4166861f, 0.8934226f, 0.6182405f
},
{ // radiance
0.01469049f, 0.01969668f, -0.02105278f, 0.2024104f, 0.1062768f, 0.1557134f
}
},
{ // turbidity 5
{ // coefficient A
-1.215034f, -1.4435f, -1.051421f, -1.034548f, -1.090576f, -1.104356f
},
{ // coefficient B
-0.2755459f, -0.519435f, -0.09990731f, -0.06777012f, -0.2542157f, -0.1955634f
},
{ // coefficient C
-0.1477751f, 0.1588243f, 0.518771f, -0.0258752f, 2.21356f, 1.782217f
},
{ // coefficient D
0.444873f, 1.429114f, -3.890856f, 10.62675f, -3.56389f, 1.085584f
},
{ // coefficient E
-0.4027817f, -2.310655f, 7.794599f, -22.66196f, 9.180955f, -5.011522f
},
{ // coefficient F
0.4787013f, 0.3033213f, 1.810628f, -2.002081f, 2.780121f, -1.07514f
},
{ // coefficient G
0.1200617f, 0.02308566f, 0.06532073f, 0.7508804f, -0.2656654f, 0.3828238f
},
{ // coefficient H
3.613995f, 1.894189f, 2.56448f, 2.022771f, -0.05012514f, 0.6133363f
},
{ // coefficient I
0.5999756f, 0.6066576f, 0.9334916f, 0.3517577f, 0.8968751f, 0.6448312f
},
{ // radiance
0.0133948f, 0.02402713f, -0.04671234f, 0.2518382f, 0.1135974f, 0.1711111f
}
},
{ // turbidity 6
{ // coefficient A
-1.261147f, -1.611753f, -0.9491151f, -1.109597f, -1.035803f, -1.132687f
},
{ // coefficient B
-0.3271435f, -0.6914768f, 0.009848038f, -0.1650703f, -0.1963707f, -0.2383551f
},
{ // coefficient C
0.113643f, 0.2911405f, 0.2711804f, -0.3529912f, 2.407913f, 1.590132f
},
{ // coefficient D
-0.003464762f, 1.806475f, -5.319331f, 12.09251f, -1.564693f, 1.025733f
},
{ // coefficient E
-3.025565f, 2.82324f, 0.1135835f, -14.35008f, 4.483717f, -4.837236f
},
{ // coefficient F
0.3660675f, 0.3320241f, 1.669815f, -2.085063f, 2.795295f, -1.334195f
},
{ // coefficient G
0.1337332f, 0.02606391f, 0.03604115f, 0.7955493f, -0.2799711f, 0.402206f
},
{ // coefficient H
3.190374f, 0.9946717f, 3.50623f, 1.240407f, 0.06851892f, 0.4281273f
},
{ // coefficient I
0.5655627f, 0.5581393f, 1.00865f, 0.2856509f, 0.9312311f, 0.6334802f
},
{ // radiance
0.01274349f, 0.02648472f, -0.06068209f, 0.2720113f, 0.1285128f, 0.1776298f
}
},
{ // turbidity 7
{ // coefficient A
-1.31024f, -1.993638f, -0.8209344f, -1.130678f, -1.032417f, -1.125604f
},
{ // coefficient B
-0.3819813f, -1.029241f, 0.1728853f, -0.3273624f, -0.1335568f, -0.232853f
},
{ // coefficient C
0.02469845f, 0.7780588f, -0.7045676f, 0.5555549f, 1.707051f, 1.537245f
},
{ // coefficient D
-0.007043983f, 1.080741f, -3.384688f, 5.800793f, 8.168228f, 4.347312f
},
{ // coefficient E
-5.009925f, 7.179598f, -4.855111f, -8.817085f, -0.005151972f, -4.368526f
},
{ // coefficient F
0.356098f, 0.2478857f, 1.661563f, -2.344445f, 2.785504f, -1.527321f
},
{ // coefficient G
0.1582176f, 0.01935012f, 0.04252574f, 0.7532869f, -0.2123703f, 0.3574251f
},
{ // coefficient H
2.966421f, -0.343424f, 5.701648f, -2.418368f, 1.770173f, 0.5982159f
},
{ // coefficient I
0.499891f, 0.4813586f, 1.110774f, 0.2201826f, 0.9064082f, 0.670588f
},
{ // radiance
0.01165402f, 0.02958682f, -0.07746524f, 0.2912132f, 0.1536569f, 0.1896419f
}
},
{ // turbidity 8
{ // coefficient A
-1.381743f, -2.362341f, -1.218829f, -0.678156f, -1.270037f, -1.046712f
},
{ // coefficient B
-0.4607543f, -1.306326f, -0.06834768f, -0.2446588f, -0.1870204f, -0.2173979f
},
{ // coefficient C
-0.2754732f, 0.6909936f, -1.24176f, 1.302166f, 0.9650201f, 1.473981f
},
{ // coefficient D
0.9134243f, -0.7196658f, 1.12289f, -2.33465f, 9.976613f, 4.368847f
},
{ // coefficient E
-0.7581174f, -0.4501812f, 5.750762f, -12.1896f, 1.444014f, -4.642506f
},
{ // coefficient F
0.6220814f, -0.05219928f, 1.231058f, -1.257952f, 1.523442f, -1.049951f
},
{ // coefficient G
0.02353878f, 0.2111683f, 0.006884716f, 0.5387955f, -0.02753646f, 0.3511771f
},
{ // coefficient H
2.891536f, -0.9499603f, 6.801032f, -5.559859f, 3.41579f, 0.2379457f
},
{ // coefficient I
0.6426078f, 0.03301774f, 1.512485f, -0.01209718f, 1.017645f, 0.6307946f
},
{ // radiance
0.01048769f, 0.031154f, -0.08550235f, 0.2769964f, 0.2173505f, 0.1991057f
}
},
{ // turbidity 9
{ // coefficient A
-1.532302f, -2.042866f, -3.413474f, 0.5838664f, -1.654226f, -0.9748612f
},
{ // coefficient B
-0.6390752f, -1.015386f, -1.48538f, 0.2370098f, -0.3296843f, -0.2545963f
},
{ // coefficient C
-0.1369123f, 0.1750126f, -1.3013f, 1.4202f, 0.6245723f, 1.374064f
},
{ // coefficient D
1.480171f, -0.99218f, 1.266203f, -1.803001f, 7.899655f, -0.002816107f
},
{ // coefficient E
-1.313391f, -0.09449241f, 3.869657f, -6.672775f, -1.079857f, -2.589259f
},
{ // coefficient F
0.5210118f, 0.4791921f, -0.01405606f, 0.2866921f, 0.3145485f, -0.5960628f
},
{ // coefficient G
-1.053608e-05f, -0.02744712f, 0.6109944f, -0.259925f, 0.4085894f, 0.2656787f
},
{ // coefficient H
2.35209f, 0.9337925f, 3.883967f, -4.162164f, 2.258895f, -0.145852f
},
{ // coefficient I
0.9000581f, -0.2902902f, 1.540321f, 0.1296901f, 0.9071309f, 0.6529731f
},
{ // radiance
0.009312926f, 0.02881725f, -0.07387358f, 0.2079818f, 0.3221838f, 0.2120657f
}
},
{ // turbidity 10
{ // coefficient A
-1.944006f, -1.294455f, -6.849943f, 1.445166f, -1.5968f, -1.035617f
},
{ // coefficient B
-0.9776516f, -0.7294996f, -2.937611f, 0.1641979f, -0.4130457f, -0.3537996f
},
{ // coefficient C
-0.2425412f, -0.01718682f, -1.10771f, 0.9591325f, 0.9250919f, 1.073337f
},
{ // coefficient D
0.818718f, 1.261264f, -3.923478f, 8.676572f, -1.406592f, 6.090616f
},
{ // coefficient E
-0.6620102f, -1.170104f, 5.018293f, -10.98801f, 2.474727f, -3.71042f
},
{ // coefficient F
0.5676187f, -0.2029198f, 2.040419f, -2.274573f, 1.748285f, -0.9390485f
},
{ // coefficient G
-7.857556e-06f, -0.01306023f, 0.2845113f, 0.134004f, 0.09929937f, 0.2233788f
},
{ // coefficient H
1.944153f, 1.821821f, 2.080902f, -1.16638f, -0.6705073f, -0.0003213961f
},
{ // coefficient I
0.9001463f, -0.253382f, 1.460328f, 0.101427f, 0.9210646f, 0.6545784f
},
{ // radiance
0.008151793f, 0.01998414f, -0.03316224f, 0.06957927f, 0.4654511f, 0.2260782f
}
}
}
},
{ // 640.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.113346f, -1.193355f, -0.9337792f, -1.297623f, -0.932527f, -1.12565f
},
{ // coefficient B
-0.1715076f, -0.2074379f, -0.2037239f, -0.1296758f, -0.1997966f, -0.1904795f
},
{ // coefficient C
-2.657094f, 1.431777f, 3.943675f, -0.3144502f, 3.363315f, 2.099998f
},
{ // coefficient D
4.63252f, 0.824557f, -2.48773f, 2.454199f, -1.887198f, 1.25193f
},
{ // coefficient E
-0.109231f, -0.1936967f, -0.9876315f, 2.433614f, -8.905212f, -1.899312f
},
{ // coefficient F
1.421516f, 0.9392137f, 2.772411f, 0.5298928f, 2.808597f, 0.5255919f
},
{ // coefficient G
0.003230348f, 0.01972523f, -0.04919462f, 0.07814939f, -0.09537165f, 0.0984826f
},
{ // coefficient H
2.697889f, 3.001209f, -4.176827f, 6.564581f, -3.059702f, 0.9627433f
},
{ // coefficient I
0.6262031f, 0.4415825f, 1.026227f, 0.5587597f, 0.5403718f, 0.6251236f
},
{ // radiance
0.01479989f, 0.009575884f, 0.02973854f, 0.04822245f, 0.03622965f, 0.03714381f
}
},
{ // turbidity 2
{ // coefficient A
-1.104956f, -1.158426f, -1.012076f, -1.200085f, -0.9836248f, -1.111842f
},
{ // coefficient B
-0.1372068f, -0.1921879f, -0.1456275f, -0.09008631f, -0.1983419f, -0.1992363f
},
{ // coefficient C
-2.996624f, 0.3371987f, 3.857821f, -1.764929f, 3.519957f, 1.746814f
},
{ // coefficient D
4.260021f, 0.8543727f, -1.13764f, 0.4584027f, -0.9777763f, 5.03617f
},
{ // coefficient E
-0.1263137f, -0.1195421f, -0.7400821f, 0.8033799f, -5.727911f, -2.993753f
},
{ // coefficient F
1.327775f, 0.2178871f, 4.132381f, -1.730728f, 4.397457f, -0.5348114f
},
{ // coefficient G
0.07595887f, 0.1573997f, -0.210147f, 0.5892683f, -0.2864495f, 0.2332738f
},
{ // coefficient H
5.301229f, 3.059646f, -0.170429f, 6.438112f, -2.511187f, 0.8970344f
},
{ // coefficient I
0.6478109f, 0.5355391f, 1.094519f, 0.2141131f, 0.9881415f, 0.5806194f
},
{ // radiance
0.01494233f, 0.01021187f, 0.02574697f, 0.07232352f, 0.03290719f, 0.04167409f
}
},
{ // turbidity 3
{ // coefficient A
-1.120087f, -1.182069f, -1.071518f, -1.113379f, -1.026698f, -1.088223f
},
{ // coefficient B
-0.1577355f, -0.2047357f, -0.20575f, -0.006708322f, -0.2260748f, -0.1817555f
},
{ // coefficient C
-1.165606f, -0.4041517f, 2.808685f, -2.181109f, 3.031659f, 1.435855f
},
{ // coefficient D
1.782016f, 1.732659f, -1.99008f, 2.107674f, -1.615765f, 1.845164f
},
{ // coefficient E
-0.1771264f, -0.4322749f, -0.8491815f, 2.21697f, -1.323341f, -3.488539f
},
{ // coefficient F
0.8928578f, 0.6549554f, 2.193915f, -0.6233552f, 2.835147f, 0.2554837f
},
{ // coefficient G
0.161311f, 0.05392767f, 0.2411077f, 0.355808f, 0.1186568f, 0.2144895f
},
{ // coefficient H
4.975693f, 4.838218f, -3.251587f, 9.251876f, -3.366247f, 1.558895f
},
{ // coefficient I
0.6258796f, 0.6441461f, 0.7470978f, 0.6319555f, 0.6850407f, 0.7315398f
},
{ // radiance
0.01434641f, 0.01147574f, 0.01506677f, 0.1142492f, 0.02532042f, 0.04995278f
}
},
{ // turbidity 4
{ // coefficient A
-1.164089f, -1.209796f, -1.232839f, -0.9291113f, -1.103614f, -1.07216f
},
{ // coefficient B
-0.2003003f, -0.2733887f, -0.2622151f, 0.04110508f, -0.2279953f, -0.1984678f
},
{ // coefficient C
-0.2998423f, -0.9360003f, 2.85705f, -2.235781f, 2.31449f, 1.092192f
},
{ // coefficient D
0.2713689f, 2.7691f, -2.866256f, 2.969741f, -0.6307971f, 0.8472734f
},
{ // coefficient E
-0.1828861f, -1.870929f, 3.128232f, -9.758544f, 3.369674f, -3.126637f
},
{ // coefficient F
0.5800779f, 0.7049322f, 1.897877f, -1.941571f, 3.787928f, -1.076607f
},
{ // coefficient G
0.1968402f, -0.08477045f, 0.4581244f, 0.3426656f, 0.2456513f, 0.3908469f
},
{ // coefficient H
4.65013f, 4.12096f, -2.112943f, 6.907158f, -1.776073f, 1.205671f
},
{ // coefficient I
0.6156174f, 0.7444509f, 0.6384882f, 0.6945892f, 0.6864316f, 0.7035925f
},
{ // radiance
0.01326292f, 0.01444403f, -0.007612369f, 0.17685f, 0.01846871f, 0.06093428f
}
},
{ // turbidity 5
{ // coefficient A
-1.222416f, -1.407746f, -1.120454f, -0.9792886f, -1.091118f, -1.053081f
},
{ // coefficient B
-0.2654298f, -0.4603243f, -0.1466269f, -0.04325464f, -0.2286983f, -0.1598496f
},
{ // coefficient C
-0.1149975f, -0.3071475f, 0.7829076f, -0.4059461f, 0.9896666f, 0.9684194f
},
{ // coefficient D
-0.006277756f, 2.93739f, -7.009271f, 15.22559f, -6.655971f, 3.620674f
},
{ // coefficient E
-1.369463f, -0.9164938f, 5.36447f, -18.67598f, 8.168041f, -5.015769f
},
{ // coefficient F
0.4402179f, 0.6696693f, 1.29428f, -1.169367f, 1.923773f, 0.08125082f
},
{ // coefficient G
0.188132f, -0.06608327f, 0.4109552f, 0.3589556f, 0.3116805f, 0.4669502f
},
{ // coefficient H
3.896722f, 2.444782f, 1.525889f, 2.368951f, 0.1852611f, 1.415221f
},
{ // coefficient I
0.6037946f, 0.6905594f, 0.7709564f, 0.5526839f, 0.7524175f, 0.7044272f
},
{ // radiance
0.01210064f, 0.01800376f, -0.03160469f, 0.2285807f, 0.02227077f, 0.06824549f
}
},
{ // turbidity 6
{ // coefficient A
-1.267172f, -1.613116f, -1.012405f, -1.022835f, -1.06837f, -1.065467f
},
{ // coefficient B
-0.3101221f, -0.6560485f, -0.02251592f, -0.136107f, -0.1806143f, -0.1928982f
},
{ // coefficient C
-0.08556526f, 0.0925027f, 0.2836289f, -0.4207218f, 1.059217f, 0.6159162f
},
{ // coefficient D
-0.003602153f, 2.119268f, -7.904006f, 19.4705f, 0.7087518f, -0.004157538f
},
{ // coefficient E
-4.988229f, 8.031058f, -9.544755f, -5.511234f, -1.671865f, -0.9397621f
},
{ // coefficient F
0.4099753f, 0.4019483f, 1.754891f, -2.417169f, 3.098645f, -1.193139f
},
{ // coefficient G
0.1555248f, 0.04526662f, 0.1283266f, 0.7837822f, -0.09859822f, 0.6979501f
},
{ // coefficient H
3.585489f, 1.362051f, 3.042478f, 0.8288902f, 0.9336021f, 1.118894f
},
{ // coefficient I
0.6128833f, 0.5999628f, 0.9234755f, 0.3509734f, 0.8929147f, 0.6467115f
},
{ // radiance
0.0113089f, 0.02150486f, -0.05124749f, 0.264802f, 0.0212306f, 0.07588774f
}
},
{ // turbidity 7
{ // coefficient A
-1.345697f, -2.096314f, -0.7573573f, -1.140929f, -1.009003f, -1.080842f
},
{ // coefficient B
-0.3906019f, -1.056514f, 0.2265213f, -0.348982f, -0.09087616f, -0.2166745f
},
{ // coefficient C
-0.1921159f, 0.7665515f, -1.283398f, 1.095429f, 0.04181128f, 0.6939662f
},
{ // coefficient D
-0.007410201f, 1.602483f, -4.696984f, 7.531589f, 11.54023f, -0.002316636f
},
{ // coefficient E
-4.683006f, 6.290779f, -4.261986f, -8.15332f, -0.5092155f, -2.616711f
},
{ // coefficient F
0.4591917f, 0.4693208f, 1.197826f, -1.722207f, 2.166116f, -1.012238f
},
{ // coefficient G
0.1919817f, -0.06258855f, 0.3444468f, 0.4427477f, 0.1620893f, 0.6132021f
},
{ // coefficient H
3.158844f, -0.3745685f, 6.004387f, -3.07244f, 2.824826f, 0.7579828f
},
{ // coefficient I
0.5319014f, 0.5853677f, 0.9413954f, 0.3874398f, 0.8289491f, 0.6813755f
},
{ // radiance
0.0102313f, 0.02633945f, -0.07626006f, 0.3042057f, 0.03007536f, 0.08597681f
}
},
{ // turbidity 8
{ // coefficient A
-1.394668f, -2.715734f, -0.8973624f, -0.8450858f, -1.180533f, -1.022908f
},
{ // coefficient B
-0.4508268f, -1.409574f, 0.02859206f, -0.1969825f, -0.2469972f, -0.1650807f
},
{ // coefficient C
-0.2615024f, 0.648511f, -1.610007f, 1.399585f, -0.02694505f, 0.3812483f
},
{ // coefficient D
-0.00741274f, 0.6307101f, -1.483136f, 0.1418708f, 16.35998f, -0.002139201f
},
{ // coefficient E
-4.811648f, 8.120123f, -5.613288f, -4.532026f, -2.378676f, -2.300711f
},
{ // coefficient F
0.4235635f, 0.3677567f, 0.7093329f, -0.743204f, 0.9477733f, -0.5917998f
},
{ // coefficient G
0.1627041f, 0.06430919f, 0.2936945f, 0.3410858f, 0.1105776f, 0.6793527f
},
{ // coefficient H
3.134174f, -0.3729191f, 5.54121f, -3.386293f, 1.954485f, 1.248273f
},
{ // coefficient I
0.5225016f, 0.346313f, 1.143952f, 0.2885614f, 0.8559872f, 0.6747565f
},
{ // radiance
0.008913856f, 0.03088076f, -0.09896179f, 0.3274618f, 0.06060024f, 0.09959796f
}
},
{ // turbidity 9
{ // coefficient A
-1.557922f, -3.061673f, -2.371583f, -0.05315727f, -1.375983f, -1.006483f
},
{ // coefficient B
-0.6294681f, -1.398157f, -1.100187f, 0.2306821f, -0.467757f, -0.1435513f
},
{ // coefficient C
-0.2355549f, 0.1597008f, -1.279309f, 1.091037f, 0.227165f, 0.05802646f
},
{ // coefficient D
-0.008743568f, 1.066059f, -3.340819f, 5.29761f, 8.771601f, -0.003181899f
},
{ // coefficient E
-5.017111f, 8.207018f, -6.556706f, -2.412315f, -2.51178f, -2.010861f
},
{ // coefficient F
0.4034446f, 0.478578f, 0.263675f, -0.07083157f, 0.1487082f, -0.1746922f
},
{ // coefficient G
0.1005643f, 0.04629487f, 0.4298705f, 0.07256586f, 0.0854221f, 0.7487809f
},
{ // coefficient H
2.735289f, 1.32402f, 2.649782f, -1.330891f, 0.005509707f, 1.623209f
},
{ // coefficient I
0.5667004f, 0.2540204f, 1.06095f, 0.4168205f, 0.7821447f, 0.6848148f
},
{ // radiance
0.007548425f, 0.03088764f, -0.09837988f, 0.2862874f, 0.145048f, 0.1121877f
}
},
{ // turbidity 10
{ // coefficient A
-1.919035f, -2.470138f, -5.53245f, 0.415915f, -1.248112f, -1.054499f
},
{ // coefficient B
-0.9396362f, -1.108577f, -2.346497f, -0.2306809f, -0.3080519f, -0.2660558f
},
{ // coefficient C
-0.3059418f, -0.5521438f, -1.734654f, 1.540502f, -0.1229235f, 0.1307025f
},
{ // coefficient D
1.542398f, -0.9047033f, 1.461689f, -1.006923f, 8.972757f, -0.002167883f
},
{ // coefficient E
-1.488273f, 0.6905739f, 1.765938f, -5.275092f, -1.532948f, -1.531056f
},
{ // coefficient F
0.5133825f, 0.37743f, 0.4969811f, -0.02776202f, -0.08283794f, 0.1525856f
},
{ // coefficient G
-7.992185e-06f, -0.02801883f, 0.6205943f, -0.1632884f, 0.1561573f, 0.5356892f
},
{ // coefficient H
2.276361f, 2.41347f, 2.112232f, -1.467491f, 0.1856113f, 0.7181428f
},
{ // coefficient I
0.8334381f, -0.08986266f, 1.159488f, 0.4177919f, 0.7696948f, 0.6837654f
},
{ // radiance
0.006525594f, 0.02429771f, -0.06656565f, 0.1679199f, 0.2792664f, 0.1313366f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.113347f, -1.193355f, -0.9337795f, -1.297624f, -0.9325285f, -1.125653f
},
{ // coefficient B
-0.1715068f, -0.2074367f, -0.2037221f, -0.1296733f, -0.1997939f, -0.1904767f
},
{ // coefficient C
-2.657094f, 1.431777f, 3.943675f, -0.3144502f, 3.363316f, 2.099998f
},
{ // coefficient D
4.63252f, 0.8245569f, -2.48773f, 2.454199f, -1.887198f, 1.25193f
},
{ // coefficient E
-0.1092308f, -0.1936966f, -0.9876315f, 2.433614f, -8.905212f, -1.899312f
},
{ // coefficient F
1.421516f, 0.9392136f, 2.77241f, 0.5298925f, 2.808596f, 0.5255917f
},
{ // coefficient G
0.003229759f, 0.01972263f, -0.04919872f, 0.07814503f, -0.09537523f, 0.09848033f
},
{ // coefficient H
2.697889f, 3.001209f, -4.176827f, 6.564581f, -3.059702f, 0.9627434f
},
{ // coefficient I
0.6262031f, 0.4415825f, 1.026227f, 0.5587597f, 0.5403717f, 0.6251233f
},
{ // radiance
0.01534297f, 0.01199131f, 0.02491229f, 0.05492441f, 0.0767965f, 0.09300994f
}
},
{ // turbidity 2
{ // coefficient A
-1.115892f, -1.147479f, -1.03045f, -1.159297f, -1.023581f, -1.102864f
},
{ // coefficient B
-0.157145f, -0.1855896f, -0.1533626f, -0.05818314f, -0.251754f, -0.1644399f
},
{ // coefficient C
-5.361081f, 2.239259f, 2.268967f, -0.148869f, 3.635148f, 2.160645f
},
{ // coefficient D
6.833528f, -0.593793f, -0.4886576f, 1.358829f, -2.37515f, 5.685422f
},
{ // coefficient E
-0.06891479f, -0.1005125f, -0.6336865f, 1.208889f, -9.409992f, -2.867248f
},
{ // coefficient F
1.309709f, 0.2773971f, 3.91108f, -1.827456f, 3.209554f, -1.020916f
},
{ // coefficient G
0.06046362f, 0.1337105f, -0.1602643f, 0.4867277f, -0.3006489f, 0.2064591f
},
{ // coefficient H
3.972373f, 3.136906f, 0.151373f, 6.18588f, -3.235953f, 1.192668f
},
{ // coefficient I
0.6651606f, 0.519423f, 1.130153f, 0.1951064f, 0.9969505f, 0.5029621f
},
{ // radiance
0.01542409f, 0.01237811f, 0.02235351f, 0.07606092f, 0.07888526f, 0.09886009f
}
},
{ // turbidity 3
{ // coefficient A
-1.120417f, -1.168618f, -1.080229f, -1.101698f, -1.047144f, -1.103227f
},
{ // coefficient B
-0.1543428f, -0.1948508f, -0.2282841f, 0.0218241f, -0.2539571f, -0.1861499f
},
{ // coefficient C
-2.173869f, -0.624756f, 2.194035f, -1.179116f, 3.684827f, 2.098952f
},
{ // coefficient D
2.965267f, 2.203402f, -1.203336f, 0.8533844f, 0.09827861f, 0.10309f
},
{ // coefficient E
-0.106219f, -0.2724794f, -0.3861545f, 0.8777171f, -0.2297266f, -4.268311f
},
{ // coefficient F
0.8112859f, 0.8364691f, 2.234045f, -1.222988f, 3.038327f, -1.12006f
},
{ // coefficient G
0.1448682f, 0.05608377f, 0.1553866f, 0.5021882f, -0.2609445f, 0.2879792f
},
{ // coefficient H
4.993622f, 4.262497f, -3.266039f, 9.364642f, -3.685112f, 0.7160893f
},
{ // coefficient I
0.6373394f, 0.6310098f, 0.8246847f, 0.4790363f, 0.8596057f, 0.6213207f
},
{ // radiance
0.01501351f, 0.01379279f, 0.01162514f, 0.1159528f, 0.07784537f, 0.1097624f
}
},
{ // turbidity 4
{ // coefficient A
-1.155288f, -1.241627f, -1.136812f, -1.028322f, -1.07683f, -1.107083f
},
{ // coefficient B
-0.2006105f, -0.2673712f, -0.2811418f, 0.09134962f, -0.3101207f, -0.1880372f
},
{ // coefficient C
-0.0582263f, -0.6444494f, 2.220215f, -1.151053f, 3.052794f, 2.00181f
},
{ // coefficient D
0.3970794f, 2.257014f, -1.53587f, 0.651162f, 0.3177593f, -0.003352202f
},
{ // coefficient E
-0.2828885f, -1.630843f, 2.354743f, -5.683098f, 3.054756f, -1.526356f
},
{ // coefficient F
0.5093569f, 0.5967071f, 2.041362f, -2.001006f, 3.386727f, -1.48055f
},
{ // coefficient G
0.1631119f, 0.009888683f, 0.2387976f, 0.6430642f, -0.2312489f, 0.3486524f
},
{ // coefficient H
4.152479f, 4.308732f, -3.453756f, 8.523367f, -3.908802f, 0.7220325f
},
{ // coefficient I
0.6357376f, 0.6751366f, 0.7672245f, 0.542224f, 0.7978614f, 0.6581821f
},
{ // radiance
0.01398757f, 0.01622679f, -0.009033929f, 0.1740587f, 0.08042195f, 0.1246928f
}
},
{ // turbidity 5
{ // coefficient A
-1.214879f, -1.424202f, -1.048548f, -1.056271f, -1.072328f, -1.107628f
},
{ // coefficient B
-0.2719222f, -0.4768943f, -0.1173837f, -0.05272364f, -0.2450563f, -0.2089585f
},
{ // coefficient C
0.2247747f, -0.639604f, 1.885902f, -1.391708f, 3.125155f, 1.799168f
},
{ // coefficient D
0.0150974f, 2.060101f, -3.700301f, 9.711492f, -2.470389f, 0.4621596f
},
{ // coefficient E
-0.5438281f, -1.11378f, 4.667326f, -17.42761f, 6.794968f, -5.008181f
},
{ // coefficient F
0.3728351f, 0.6993089f, 1.344798f, -1.677628f, 2.678049f, -1.121127f
},
{ // coefficient G
0.174988f, -0.02098291f, 0.2140261f, 0.6906304f, -0.2092059f, 0.3917217f
},
{ // coefficient H
3.13222f, 2.504826f, 0.1525414f, 4.107662f, -1.651858f, 0.2703209f
},
{ // coefficient I
0.6001432f, 0.6757213f, 0.8450168f, 0.4268005f, 0.870874f, 0.6449597f
},
{ // radiance
0.01276036f, 0.02055104f, -0.03597123f, 0.2310287f, 0.08109251f, 0.1421324f
}
},
{ // turbidity 6
{ // coefficient A
-1.249741f, -1.577209f, -1.015394f, -1.030222f, -1.086063f, -1.113156f
},
{ // coefficient B
-0.3073987f, -0.6213162f, -0.09306378f, -0.03712071f, -0.2709359f, -0.2162986f
},
{ // coefficient C
0.1398829f, 0.2895282f, 0.4870291f, -0.4565649f, 2.527868f, 1.815038f
},
{ // coefficient D
-0.002878563f, 1.670732f, -6.558243f, 18.27249f, -1.786375f, 0.1036606f
},
{ // coefficient E
-5.008845f, 8.708786f, -10.73347f, -4.505703f, -2.329338f, -1.106537f
},
{ // coefficient F
0.4535605f, 0.2480283f, 1.842159f, -2.63523f, 3.367729f, -1.657558f
},
{ // coefficient G
0.1447766f, 0.07326386f, 0.02135043f, 0.9447498f, -0.420167f, 0.4923976f
},
{ // coefficient H
3.091173f, 1.395638f, 1.655269f, 2.450865f, -0.9372873f, -0.01768985f
},
{ // coefficient I
0.6138128f, 0.5814691f, 0.9750505f, 0.2757082f, 0.9570917f, 0.6218745f
},
{ // radiance
0.01198617f, 0.02326685f, -0.05243143f, 0.2599544f, 0.08840775f, 0.1517899f
}
},
{ // turbidity 7
{ // coefficient A
-1.345662f, -1.998866f, -0.7854213f, -1.113558f, -1.077319f, -1.101702f
},
{ // coefficient B
-0.4038158f, -0.9786972f, 0.1186171f, -0.1964692f, -0.2429937f, -0.2020333f
},
{ // coefficient C
0.1601105f, 0.576325f, -0.1925047f, -0.06175963f, 2.226652f, 1.63767f
},
{ // coefficient D
-0.006892546f, 1.007104f, -3.567464f, 7.345047f, 8.082352f, 0.5276716f
},
{ // coefficient E
-5.010308f, 8.983795f, -8.901606f, -5.293662f, -0.5647382f, -4.006007f
},
{ // coefficient F
0.3903794f, 0.375211f, 1.181923f, -1.720046f, 2.187018f, -1.066501f
},
{ // coefficient G
0.1543535f, 0.07375525f, 0.04049067f, 0.8863695f, -0.3571678f, 0.4548015f
},
{ // coefficient H
2.538245f, 0.1792659f, 3.603602f, -0.1699694f, -0.237089f, 0.4055769f
},
{ // coefficient I
0.5555141f, 0.4969463f, 1.08155f, 0.2184709f, 0.9528157f, 0.6485706f
},
{ // radiance
0.01092344f, 0.02669375f, -0.07074274f, 0.28303f, 0.1143047f, 0.1610563f
}
},
{ // turbidity 8
{ // coefficient A
-1.411469f, -2.735007f, -0.7371503f, -0.967837f, -1.148918f, -1.090291f
},
{ // coefficient B
-0.4870956f, -1.453253f, 0.1290592f, -0.256283f, -0.2271366f, -0.2315314f
},
{ // coefficient C
0.1017578f, 0.5783189f, -0.5349577f, 0.2961987f, 1.81789f, 1.491197f
},
{ // coefficient D
-0.008806763f, 0.5673243f, -1.885854f, 2.461144f, 10.97345f, 9.632479f
},
{ // coefficient E
-5.014847f, 9.968637f, -8.623054f, -4.753928f, -1.12616f, -4.323932f
},
{ // coefficient F
0.4306981f, 0.2236397f, 1.078614f, -1.657373f, 1.993895f, -1.326631f
},
{ // coefficient G
0.1278033f, 0.1860109f, -0.05694996f, 0.8388825f, -0.2870862f, 0.3604933f
},
{ // coefficient H
2.361142f, -0.3781953f, 4.526279f, -2.440974f, 0.7269217f, 0.1905995f
},
{ // coefficient I
0.5472925f, 0.2505082f, 1.347569f, 0.01816225f, 1.032783f, 0.6312241f
},
{ // radiance
0.009699402f, 0.0295335f, -0.08582613f, 0.2867195f, 0.1638123f, 0.1760228f
}
},
{ // turbidity 9
{ // coefficient A
-1.552618f, -2.770597f, -2.631791f, 0.1875824f, -1.489693f, -1.042349f
},
{ // coefficient B
-0.6336918f, -1.302036f, -1.247576f, 0.3564184f, -0.4947623f, -0.2134514f
},
{ // coefficient C
0.04605315f, 0.3421676f, -0.842394f, 0.7973268f, 1.253918f, 1.289359f
},
{ // coefficient D
-0.008452985f, 1.220769f, -3.955998f, 7.15916f, 3.949282f, -0.003129206f
},
{ // coefficient E
-4.979015f, 7.584372f, -5.3411f, -3.85734f, -1.320903f, -2.385397f
},
{ // coefficient F
0.3484674f, 0.4050789f, 0.6385142f, -0.9227702f, 1.088652f, -0.7642278f
},
{ // coefficient G
0.08791637f, 0.04478808f, 0.3872948f, 0.08283793f, 0.1489763f, 0.3772293f
},
{ // coefficient H
2.297399f, 0.943571f, 2.279807f, -1.444145f, -0.3855853f, 0.3683001f
},
{ // coefficient I
0.570523f, 0.2393878f, 1.101259f, 0.3654244f, 0.813096f, 0.6811674f
},
{ // radiance
0.008432894f, 0.02784449f, -0.07740437f, 0.2250221f, 0.2667103f, 0.1889583f
}
},
{ // turbidity 10
{ // coefficient A
-1.924886f, -1.556107f, -7.169297f, 1.889259f, -1.816439f, -0.9684886f
},
{ // coefficient B
-0.9354584f, -0.820041f, -2.972878f, 0.260011f, -0.3601966f, -0.3866519f
},
{ // coefficient C
-0.1879067f, -0.1065249f, -1.324108f, 1.309033f, 0.6367922f, 1.268224f
},
{ // coefficient D
1.267892f, -0.3241942f, 0.007276378f, 1.187864f, 6.79816f, -0.004187273f
},
{ // coefficient E
-1.174622f, 0.1552504f, 2.638975f, -6.905754f, -0.6431562f, -1.833316f
},
{ // coefficient F
0.5570232f, 0.07662815f, 1.18615f, -1.163025f, 0.9014269f, -0.4626197f
},
{ // coefficient G
-1.060836e-05f, -0.02568474f, 0.5695709f, -0.1642758f, 0.2490693f, 0.2263859f
},
{ // coefficient H
2.002481f, 1.738474f, 2.027718f, -1.774036f, -0.006685128f, -0.5583963f
},
{ // coefficient I
0.9001398f, -0.2100942f, 1.328077f, 0.2566466f, 0.8542883f, 0.6650112f
},
{ // radiance
0.007395069f, 0.0203182f, -0.04154554f, 0.09577204f, 0.4084819f, 0.2043633f
}
}
}
},
{ // 680.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.112655f, -1.138469f, -1.026337f, -1.198321f, -0.9935091f, -1.092682f
},
{ // coefficient B
-0.1844098f, -0.1797086f, -0.1861539f, -0.1585305f, -0.1747698f, -0.1840784f
},
{ // coefficient C
-3.170582f, 1.271179f, 2.119648f, 1.339219f, 2.816779f, 2.455887f
},
{ // coefficient D
5.334685f, 1.158372f, -0.67532f, 0.5796904f, -0.3496148f, 0.1441429f
},
{ // coefficient E
-0.06690891f, -0.1687824f, -0.19727f, -0.05143433f, 1.832218f, -4.002347f
},
{ // coefficient F
1.561122f, 1.414051f, 2.384659f, 1.107455f, 2.324658f, 1.186375f
},
{ // coefficient G
-2.792088e-06f, 0.004258569f, -0.01304972f, 0.02551349f, -0.03408305f, 0.03634755f
},
{ // coefficient H
1.400688f, 2.135675f, -1.804518f, 3.346946f, -1.618451f, 0.3070963f
},
{ // coefficient I
0.6639418f, 0.5322718f, 1.207802f, -0.03387602f, 1.189512f, 0.5512501f
},
{ // radiance
0.01320908f, 0.009179272f, 0.02540842f, 0.03413687f, 0.02736575f, 0.02799241f
}
},
{ // turbidity 2
{ // coefficient A
-1.104975f, -1.13157f, -1.030016f, -1.167563f, -1.018402f, -1.078985f
},
{ // coefficient B
-0.1425541f, -0.1500053f, -0.1667752f, -0.03644294f, -0.2068288f, -0.1995548f
},
{ // coefficient C
-1.889148f, -1.048539f, 6.1796f, -4.501893f, 4.710566f, 1.678948f
},
{ // coefficient D
3.408593f, 3.133642f, -5.163198f, 4.471663f, -1.425858f, 0.3074167f
},
{ // coefficient E
-0.1252648f, -0.2651219f, -0.4084328f, -0.5377956f, 0.1287815f, -1.64283f
},
{ // coefficient F
1.396095f, 1.193999f, 2.617798f, 0.04971351f, 3.605281f, -0.123547f
},
{ // coefficient G
0.08358812f, 0.07013543f, 0.001966627f, 0.4621337f, -0.2164599f, 0.2735898f
},
{ // coefficient H
3.814297f, 4.592749f, -2.417731f, 10.14187f, -4.336932f, 0.6360197f
},
{ // coefficient I
0.6848154f, 0.685775f, 0.840231f, 0.4411276f, 0.9051639f, 0.5870665f
},
{ // radiance
0.01364418f, 0.009639315f, 0.02455688f, 0.05468867f, 0.024232f, 0.03240293f
}
},
{ // turbidity 3
{ // coefficient A
-1.132881f, -1.138356f, -1.093982f, -1.117812f, -1.013135f, -1.087086f
},
{ // coefficient B
-0.1676238f, -0.1681053f, -0.1847904f, -0.06182272f, -0.1529086f, -0.22319f
},
{ // coefficient C
-1.179974f, 1.54832f, 1.234133f, -0.4681118f, 1.960772f, 1.871061f
},
{ // coefficient D
1.90208f, -0.6001024f, -0.4707125f, 5.075227f, -3.022528f, 1.830836f
},
{ // coefficient E
-0.07980402f, -0.4138892f, 1.076579f, -15.61122f, 8.938126f, -4.39714f
},
{ // coefficient F
0.8946611f, 0.2586807f, 3.355751f, -2.343845f, 4.411437f, -0.5441402f
},
{ // coefficient G
0.1939671f, 0.2126762f, 0.053946f, 0.5618788f, 0.03156643f, 0.2728682f
},
{ // coefficient H
3.793414f, 4.742849f, -2.31491f, 7.005713f, -1.44918f, -0.05607736f
},
{ // coefficient I
0.6357246f, 0.5910253f, 0.8900752f, 0.4622191f, 0.8181914f, 0.6741811f
},
{ // radiance
0.01356642f, 0.009864007f, 0.02017166f, 0.08738638f, 0.02138981f, 0.03828822f
}
},
{ // turbidity 4
{ // coefficient A
-1.176035f, -1.14018f, -1.282085f, -0.9092985f, -1.110248f, -1.0549f
},
{ // coefficient B
-0.2099056f, -0.191848f, -0.3059464f, 0.04843416f, -0.2047143f, -0.2018466f
},
{ // coefficient C
-0.8221907f, 0.2844393f, 1.619956f, -1.076741f, 1.565253f, 1.352034f
},
{ // coefficient D
1.520994f, -0.3842927f, -1.362117f, 8.143038f, -4.575474f, 2.656066f
},
{ // coefficient E
-0.2507738f, -0.5520764f, 2.324649f, -16.18418f, 8.737741f, -4.403444f
},
{ // coefficient F
0.9100863f, -0.1868484f, 3.350678f, -3.681285f, 5.023419f, -1.479998f
},
{ // coefficient G
0.1507234f, 0.2247948f, 0.2102082f, 0.52178f, 0.246923f, 0.3939305f
},
{ // coefficient H
3.661138f, 5.381202f, -4.028753f, 7.307672f, -1.716906f, 0.6455032f
},
{ // coefficient I
0.6726424f, 0.6059726f, 0.8172091f, 0.5312006f, 0.7856742f, 0.6775162f
},
{ // radiance
0.01274664f, 0.01046958f, 0.007076299f, 0.1378101f, 0.02058352f, 0.04607358f
}
},
{ // turbidity 5
{ // coefficient A
-1.237521f, -1.354583f, -1.1525f, -0.9782313f, -1.064286f, -1.067798f
},
{ // coefficient B
-0.2734963f, -0.4251172f, -0.1277879f, -0.08253891f, -0.1710957f, -0.1817268f
},
{ // coefficient C
-0.4554763f, -0.3241668f, 1.089367f, -0.567347f, 0.9336921f, 1.09572f
},
{ // coefficient D
0.3417711f, 2.077967f, -4.49752f, 10.98032f, -5.525059f, 3.551145f
},
{ // coefficient E
0.2054204f, -1.91041f, 5.636501f, -16.8955f, 8.053084f, -5.009504f
},
{ // coefficient F
0.395447f, 0.8755633f, 1.249272f, -1.619205f, 2.664602f, -0.4802047f
},
{ // coefficient G
0.2366979f, -0.08365829f, 0.5829396f, 0.212566f, 0.4815015f, 0.4854159f
},
{ // coefficient H
3.239693f, 2.443727f, 0.9198423f, 2.080566f, 0.4884965f, 0.8021585f
},
{ // coefficient I
0.6083737f, 0.7260119f, 0.7039872f, 0.6315535f, 0.7093015f, 0.7133339f
},
{ // radiance
0.01151617f, 0.01429568f, -0.01804673f, 0.1974118f, 0.01313176f, 0.05733295f
}
},
{ // turbidity 6
{ // coefficient A
-1.251896f, -1.615603f, -0.9703572f, -1.053437f, -1.051388f, -1.060369f
},
{ // coefficient B
-0.293651f, -0.6405318f, 0.01437971f, -0.1577255f, -0.1690749f, -0.1751603f
},
{ // coefficient C
-0.03090944f, -0.4651954f, 0.6532383f, -0.2354526f, 0.6925698f, 0.867829f
},
{ // coefficient D
-0.005488869f, 2.332689f, -6.450045f, 17.53047f, -7.450365f, 5.136962f
},
{ // coefficient E
-0.6577788f, -0.1893131f, 4.02638f, -18.02766f, 7.74778f, -5.014566f
},
{ // coefficient F
0.4577691f, 0.339117f, 2.198628f, -3.193379f, 3.68196f, -1.283438f
},
{ // coefficient G
0.1780002f, 0.03495888f, 0.2684967f, 0.626438f, 0.1688173f, 0.6547844f
},
{ // coefficient H
3.335561f, 1.058634f, 2.873804f, 0.3733714f, 0.9054528f, 0.9420523f
},
{ // coefficient I
0.6394482f, 0.6310524f, 0.8626816f, 0.4466776f, 0.8229209f, 0.6738511f
},
{ // radiance
0.01089311f, 0.01719127f, -0.03582843f, 0.2315641f, 0.0140294f, 0.06289477f
}
},
{ // turbidity 7
{ // coefficient A
-1.358251f, -2.043506f, -0.7280672f, -1.155299f, -1.006427f, -1.068759f
},
{ // coefficient B
-0.3909003f, -0.9923881f, 0.2228069f, -0.3079453f, -0.1250087f, -0.1729191f
},
{ // coefficient C
-0.07590093f, -0.4530797f, 0.2169765f, -0.1640569f, 0.7477012f, 0.5265652f
},
{ // coefficient D
-0.008010253f, 1.750575f, -3.681736f, 7.663297f, 2.612704f, 7.074812f
},
{ // coefficient E
-0.2417641f, -0.4362236f, 4.308793f, -13.27817f, 2.531799f, -4.259958f
},
{ // coefficient F
0.5031789f, 0.2997323f, 1.308622f, -1.431157f, 1.565371f, -0.4833255f
},
{ // coefficient G
0.1882499f, 0.02328504f, 0.3451835f, 0.4096359f, 0.3452604f, 0.604338f
},
{ // coefficient H
2.80706f, 0.1626543f, 4.037459f, -0.9185502f, 0.9129266f, 1.489203f
},
{ // coefficient I
0.5884485f, 0.5806819f, 0.9113772f, 0.4578034f, 0.7684386f, 0.7089074f
},
{ // radiance
0.009705879f, 0.02218462f, -0.06221073f, 0.2771148f, 0.01651339f, 0.07346287f
}
},
{ // turbidity 8
{ // coefficient A
-1.398983f, -2.970455f, -0.3942086f, -1.181174f, -1.060281f, -1.037564f
},
{ // coefficient B
-0.4500153f, -1.529948f, 0.3424842f, -0.4197175f, -0.1080598f, -0.2018083f
},
{ // coefficient C
-0.5189309f, 0.1094532f, -1.253259f, 1.429102f, -0.327648f, 0.6250668f
},
{ // coefficient D
0.1546325f, 0.7382263f, -0.9268654f, -0.007894317f, 13.6925f, -0.002525843f
},
{ // coefficient E
0.4187327f, -1.024683f, 5.636065f, -12.39152f, 1.504389f, -3.269998f
},
{ // coefficient F
0.2811516f, 0.2074091f, 0.8224289f, -0.5753431f, 0.6401191f, -0.2649814f
},
{ // coefficient G
0.2379667f, 0.1397605f, 0.2452235f, 0.355218f, 0.2437501f, 0.6600589f
},
{ // coefficient H
2.80104f, -0.6743828f, 5.864258f, -4.495555f, 3.212918f, 0.3337508f
},
{ // coefficient I
0.5198375f, 0.3496944f, 1.20282f, 0.2141883f, 0.9172133f, 0.6516131f
},
{ // radiance
0.008356372f, 0.02724501f, -0.08836051f, 0.3099528f, 0.04114206f, 0.08518889f
}
},
{ // turbidity 9
{ // coefficient A
-1.610481f, -3.073343f, -2.207768f, -0.01981173f, -1.435873f, -0.9812568f
},
{ // coefficient B
-0.6353902f, -1.365905f, -1.009608f, 0.2018414f, -0.3390505f, -0.2273751f
},
{ // coefficient C
-0.5968977f, -0.2651943f, -1.329601f, 1.457765f, -0.3859047f, 0.5108069f
},
{ // coefficient D
1.414037f, -1.68531f, 3.393091f, -6.316169f, 14.57464f, -0.003735693f
},
{ // coefficient E
-0.7941453f, 0.4755874f, 3.419104f, -6.640569f, -1.121591f, -2.547988f
},
{ // coefficient F
0.7888684f, -0.1110669f, -0.04733375f, 0.7150646f, -0.06089868f, -0.4492561f
},
{ // coefficient G
0.02209045f, 0.2561961f, 0.5989185f, -0.2993748f, 0.531922f, 0.5069199f
},
{ // coefficient H
2.615106f, 1.222045f, 2.814641f, -2.703376f, 1.852395f, 0.297791f
},
{ // coefficient I
0.687026f, 0.0336278f, 1.245189f, 0.3218596f, 0.8379521f, 0.6639273f
},
{ // radiance
0.007026434f, 0.02944084f, -0.09824183f, 0.2940735f, 0.1048477f, 0.1012199f
}
},
{ // turbidity 10
{ // coefficient A
-2.056405f, -2.105651f, -6.179767f, 1.240848f, -1.539149f, -1.036841f
},
{ // coefficient B
-0.9703512f, -0.8924283f, -2.65158f, 0.1126706f, -0.208189f, -0.4359099f
},
{ // coefficient C
-0.3731953f, -0.6868702f, -1.527652f, 1.620422f, -0.4552234f, 0.4767834f
},
{ // coefficient D
2.052022f, -1.665838f, 2.692054f, -3.384604f, 10.5641f, -0.003794262f
},
{ // coefficient E
-1.445283f, 0.4795702f, 2.293427f, -5.545532f, -1.048637f, -1.980492f
},
{ // coefficient F
0.6103743f, 0.522808f, -0.1005611f, 0.4928203f, -0.2780921f, 0.09954067f
},
{ // coefficient G
-9.389644e-06f, -0.04272483f, 0.9545323f, -0.5999585f, 0.5682296f, 0.3037557f
},
{ // coefficient H
2.198652f, 2.661317f, 1.367438f, -1.407078f, 0.6785092f, 0.02156592f
},
{ // coefficient I
0.8567335f, -0.1154062f, 1.169193f, 0.4193816f, 0.7729892f, 0.6845201f
},
{ // radiance
0.005935885f, 0.0242907f, -0.07239967f, 0.1895904f, 0.2306596f, 0.1177261f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.11098f, -1.132332f, -1.015776f, -1.189845f, -1.000281f, -1.120411f
},
{ // coefficient B
-0.1799491f, -0.1786787f, -0.1825058f, -0.1509036f, -0.1696328f, -0.1974172f
},
{ // coefficient C
-3.1686f, 1.272954f, 2.122848f, 1.345709f, 2.829408f, 2.474172f
},
{ // coefficient D
5.336662f, 1.160211f, -0.6725506f, 0.5832609f, -0.3466041f, 0.1453506f
},
{ // coefficient E
-0.06318131f, -0.1685885f, -0.1976294f, -0.05130839f, 1.832532f, -4.002206f
},
{ // coefficient F
1.559251f, 1.40955f, 2.376807f, 1.097035f, 2.314637f, 1.180333f
},
{ // coefficient G
0.003204341f, 0.007140682f, -0.01859856f, 0.02686911f, -0.02721994f, 0.02229171f
},
{ // coefficient H
1.40114f, 2.136473f, -1.803033f, 3.349361f, -1.614939f, 0.3112615f
},
{ // coefficient I
0.6638144f, 0.5320908f, 1.207528f, -0.03431297f, 1.18872f, 0.5505539f
},
{ // radiance
0.01370741f, 0.01011873f, 0.02499587f, 0.0336288f, 0.0641675f, 0.06673258f
}
},
{ // turbidity 2
{ // coefficient A
-1.112667f, -1.113577f, -1.061651f, -1.136633f, -1.027909f, -1.102843f
},
{ // coefficient B
-0.1574855f, -0.1331062f, -0.2007134f, -0.004281875f, -0.2334388f, -0.1875642f
},
{ // coefficient C
-2.284814f, -1.828757f, 5.907472f, -3.407541f, 4.972147f, 2.08776f
},
{ // coefficient D
4.25505f, 3.910596f, -4.808769f, 4.132332f, -1.499736f, 0.36926f
},
{ // coefficient E
-0.1386097f, -0.2095519f, -0.2120206f, -0.3866153f, 0.09598438f, -0.6520835f
},
{ // coefficient F
1.459506f, 1.159335f, 2.727568f, -0.5082717f, 2.744895f, -0.9598766f
},
{ // coefficient G
0.04508931f, 0.09311608f, -0.04222666f, 0.4828348f, -0.3558702f, 0.2984231f
},
{ // coefficient H
2.944474f, 4.517295f, -2.599674f, 9.797167f, -4.288054f, 0.4045786f
},
{ // coefficient I
0.7339984f, 0.6365313f, 0.9265659f, 0.3325034f, 1.000138f, 0.5091931f
},
{ // radiance
0.01417558f, 0.01065771f, 0.02402602f, 0.05330606f, 0.06505571f, 0.07324558f
}
},
{ // turbidity 3
{ // coefficient A
-1.117485f, -1.123872f, -1.118262f, -1.095394f, -1.035035f, -1.113497f
},
{ // coefficient B
-0.1492968f, -0.1467158f, -0.246205f, 0.0200968f, -0.2296018f, -0.19314f
},
{ // coefficient C
-2.639425f, 3.049045f, -0.08333919f, 0.8468659f, 2.722309f, 2.288509f
},
{ // coefficient D
3.672106f, -2.607634f, 2.438788f, 0.01230624f, 0.1849369f, 0.04973129f
},
{ // coefficient E
-0.1058342f, -0.2505495f, 0.6143236f, -8.638525f, 3.440975f, -0.1353292f
},
{ // coefficient F
1.098101f, -0.08310458f, 3.745903f, -2.989487f, 4.372083f, -1.587772f
},
{ // coefficient G
0.1544782f, 0.3333822f, -0.2043295f, 0.8735734f, -0.4747494f, 0.4073801f
},
{ // coefficient H
4.496691f, 4.631418f, -3.451983f, 8.53374f, -2.984092f, -0.008180513f
},
{ // coefficient I
0.6582312f, 0.5120491f, 1.056003f, 0.2548062f, 1.009737f, 0.5682569f
},
{ // radiance
0.01406012f, 0.01182138f, 0.01636956f, 0.09161153f, 0.0603184f, 0.08638638f
}
},
{ // turbidity 4
{ // coefficient A
-1.172242f, -1.167834f, -1.222298f, -0.9554071f, -1.119763f, -1.084261f
},
{ // coefficient B
-0.2084847f, -0.2117291f, -0.2893306f, 0.06610246f, -0.2579942f, -0.208657f
},
{ // coefficient C
-1.212027f, 1.196578f, 0.9080116f, -0.3323811f, 2.830634f, 2.248616f
},
{ // coefficient D
1.991627f, -1.438719f, 1.413518f, 2.702543f, -1.333418f, 0.6993871f
},
{ // coefficient E
-0.1403495f, -0.3137939f, 1.709237f, -13.45592f, 6.825773f, -2.39176f
},
{ // coefficient F
0.7684996f, -0.04537552f, 2.915962f, -3.119309f, 4.230596f, -1.738827f
},
{ // coefficient G
0.1582914f, 0.2260096f, 0.08464584f, 0.7740473f, -0.2654108f, 0.3674794f
},
{ // coefficient H
3.407597f, 4.659389f, -3.867076f, 7.452516f, -2.669457f, -0.5324741f
},
{ // coefficient I
0.6659386f, 0.5776369f, 0.9110543f, 0.3929172f, 0.9126183f, 0.6183292f
},
{ // radiance
0.01321071f, 0.01310944f, 0.001017467f, 0.145004f, 0.06291618f, 0.1003625f
}
},
{ // turbidity 5
{ // coefficient A
-1.226325f, -1.359737f, -1.088212f, -1.034524f, -1.07615f, -1.103432f
},
{ // coefficient B
-0.2688667f, -0.3985716f, -0.1688766f, -0.01050729f, -0.2551781f, -0.2058047f
},
{ // coefficient C
-0.146204f, -0.1570034f, 1.399098f, -0.8054376f, 2.79779f, 2.11713f
},
{ // coefficient D
0.4749868f, 1.277905f, -3.25495f, 11.66351f, -5.43795f, 2.401558f
},
{ // coefficient E
-0.08718681f, -1.314322f, 4.733557f, -18.79498f, 9.156159f, -5.01537f
},
{ // coefficient F
0.4956704f, 0.4320873f, 2.079183f, -3.021428f, 3.988462f, -1.82948f
},
{ // coefficient G
0.1752427f, 0.08532731f, 0.190799f, 0.7755251f, -0.2324172f, 0.4046378f
},
{ // coefficient H
2.933911f, 2.774395f, -0.9540048f, 4.516574f, -2.124766f, -0.2078794f
},
{ // coefficient I
0.6470198f, 0.6318556f, 0.8714774f, 0.409269f, 0.8863276f, 0.6449045f
},
{ // radiance
0.0119491f, 0.01687076f, -0.02332212f, 0.2021394f, 0.06093196f, 0.1177929f
}
},
{ // turbidity 6
{ // coefficient A
-1.263516f, -1.577927f, -0.9191335f, -1.144508f, -1.018834f, -1.12523f
},
{ // coefficient B
-0.3133966f, -0.5893907f, -0.04529201f, -0.09289247f, -0.2186851f, -0.2422532f
},
{ // coefficient C
0.317456f, -0.2991531f, 1.577262f, -1.606387f, 3.321767f, 1.850916f
},
{ // coefficient D
-0.004989246f, 1.783728f, -6.455191f, 20.61805f, -7.887012f, 6.213105f
},
{ // coefficient E
-2.31763f, 3.693827f, -4.020064f, -10.51527f, 3.374834f, -5.017914f
},
{ // coefficient F
0.3963746f, 0.5704918f, 1.309911f, -2.077164f, 3.005232f, -1.644457f
},
{ // coefficient G
0.1677262f, 0.06768069f, 0.1373281f, 0.8328239f, -0.2515037f, 0.3811828f
},
{ // coefficient H
2.563898f, 2.21518f, -0.7178508f, 4.729795f, -2.65661f, -0.1772362f
},
{ // coefficient I
0.6335899f, 0.6196162f, 0.9243738f, 0.3298487f, 0.9316017f, 0.629736f
},
{ // radiance
0.01126778f, 0.0190545f, -0.03804088f, 0.2298767f, 0.07038318f, 0.1240367f
}
},
{ // turbidity 7
{ // coefficient A
-1.365631f, -1.957177f, -0.7832629f, -1.121562f, -1.059207f, -1.108877f
},
{ // coefficient B
-0.4017085f, -0.9640834f, 0.1949904f, -0.2704893f, -0.1613941f, -0.2404852f
},
{ // coefficient C
0.1942501f, -0.2217521f, 0.6232354f, -0.5161748f, 2.375866f, 1.830175f
},
{ // coefficient D
-0.007073927f, 1.316645f, -2.947737f, 8.115427f, -0.09517705f, 2.808956f
},
{ // coefficient E
-0.1461125f, -0.1907085f, 3.702377f, -14.43497f, 4.514722f, -4.932362f
},
{ // coefficient F
0.4898017f, 0.01134191f, 2.212239f, -3.292676f, 3.755824f, -2.042775f
},
{ // coefficient G
0.1623346f, 0.09704476f, 0.09128296f, 0.8232212f, -0.1696953f, 0.4168739f
},
{ // coefficient H
2.330174f, 0.03828565f, 3.131504f, -0.01548451f, -0.2539157f, -0.253459f
},
{ // coefficient I
0.5983242f, 0.5538419f, 0.9923939f, 0.3199244f, 0.8992553f, 0.6533212f
},
{ // radiance
0.01025282f, 0.02337214f, -0.06236283f, 0.2708195f, 0.07801793f, 0.1406577f
}
},
{ // turbidity 8
{ // coefficient A
-1.405398f, -2.843953f, -0.5156586f, -1.070353f, -1.136211f, -1.083101f
},
{ // coefficient B
-0.4540068f, -1.562366f, 0.3789775f, -0.4037269f, -0.1473182f, -0.246891f
},
{ // coefficient C
-0.4766512f, 0.8527921f, -1.194407f, 1.108932f, 1.235769f, 1.894919f
},
{ // coefficient D
0.1871207f, 0.5991182f, -0.7591558f, 1.269159f, 5.725364f, 6.574012f
},
{ // coefficient E
0.553507f, -0.9984088f, 4.515532f, -10.99135f, 1.712036f, -4.949098f
},
{ // coefficient F
-0.04467365f, 0.7599314f, 0.4795814f, -1.049849f, 1.630777f, -1.132033f
},
{ // coefficient G
0.2846366f, -0.02025184f, 0.3163186f, 0.4644725f, 0.03656685f, 0.3336584f
},
{ // coefficient H
2.443422f, -1.51505f, 5.993132f, -4.387074f, 2.082798f, -0.7321133f
},
{ // coefficient I
0.5035706f, 0.4397429f, 1.096327f, 0.2704008f, 0.9036801f, 0.6606587f
},
{ // radiance
0.009018216f, 0.0271884f, -0.08215497f, 0.2866465f, 0.1194106f, 0.1556975f
}
},
{ // turbidity 9
{ // coefficient A
-1.60991f, -3.047283f, -2.160824f, 0.01444817f, -1.480512f, -1.016066f
},
{ // coefficient B
-0.655038f, -1.421592f, -0.9212504f, 0.1418426f, -0.3141638f, -0.2346375f
},
{ // coefficient C
-0.5773346f, -0.4003615f, -0.843842f, 0.7055176f, 1.193403f, 1.457587f
},
{ // coefficient D
1.426156f, -0.6531692f, 1.466021f, -1.787432f, 6.152773f, -0.002761458f
},
{ // coefficient E
-0.5000686f, 0.5643821f, 1.124686f, -1.625625f, -3.314867f, -2.546952f
},
{ // coefficient F
0.8257866f, -0.2693762f, -0.06848173f, 0.7638401f, 0.1345353f, -0.6058542f
},
{ // coefficient G
0.01839148f, 0.2295553f, 0.6224893f, -0.330994f, 0.5359935f, 0.313423f
},
{ // coefficient H
2.106656f, 0.553391f, 3.137662f, -3.414755f, 1.409582f, -0.2232281f
},
{ // coefficient I
0.6957214f, 0.02751822f, 1.224147f, 0.3580691f, 0.8072343f, 0.6760089f
},
{ // radiance
0.007747698f, 0.02701679f, -0.08076599f, 0.2419548f, 0.2126954f, 0.1683085f
}
},
{ // turbidity 10
{ // coefficient A
-2.019395f, -2.071307f, -6.388445f, 1.742851f, -1.845177f, -0.9599955f
},
{ // coefficient B
-0.9550942f, -0.9485438f, -2.738739f, 0.2950102f, -0.3242819f, -0.3732025f
},
{ // coefficient C
-0.1187885f, -0.3817151f, -0.9872469f, 1.015937f, 0.7060963f, 1.381129f
},
{ // coefficient D
1.995372f, -1.657017f, 2.450058f, -2.511194f, 7.747868f, -0.003983023f
},
{ // coefficient E
-1.604228f, 0.9488626f, 1.075999f, -3.829361f, -1.721532f, -1.941446f
},
{ // coefficient F
0.4995785f, 0.5816145f, 0.02305434f, -0.1126698f, 0.6057347f, -0.594349f
},
{ // coefficient G
-9.647137e-06f, -0.03856514f, 0.8598771f, -0.4564605f, 0.4266212f, 0.2269207f
},
{ // coefficient H
1.929333f, 1.677756f, 1.853582f, -2.097128f, 0.3153697f, -0.7249738f
},
{ // coefficient I
0.8659254f, -0.1262572f, 1.180017f, 0.4012648f, 0.7958222f, 0.6710888f
},
{ // radiance
0.006701188f, 0.0208544f, -0.05068221f, 0.1256935f, 0.3480598f, 0.1851691f
}
}
}
},
{ // 720.000000nm
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.110553f, -1.14996f, -0.970243f, -1.278773f, -0.9511618f, -1.095577f
},
{ // coefficient B
-0.1675726f, -0.2418229f, -0.05384657f, -0.2072018f, -0.1674143f, -0.1855865f
},
{ // coefficient C
-0.2349324f, 2.341927f, 3.025898f, -0.2458359f, 4.195842f, 1.973593f
},
{ // coefficient D
2.43379f, 0.1486508f, -0.6450284f, 0.3749133f, -0.1205785f, 0.07894097f
},
{ // coefficient E
-0.1342878f, -0.4333388f, 0.9728166f, -9.580484f, 2.160133f, 0.9578182f
},
{ // coefficient F
1.755516f, 0.902874f, 3.532967f, 0.306602f, 3.048813f, 0.8565674f
},
{ // coefficient G
-1.879493e-06f, 0.00945071f, -0.02429753f, 0.04056471f, -0.04826924f, 0.04663966f
},
{ // coefficient H
1.509217f, -0.313479f, 1.343614f, 4.343208f, -3.779096f, 0.7375811f
},
{ // coefficient I
0.7216495f, 0.5923234f, 1.067807f, 0.1865704f, 1.024834f, 0.5577192f
},
{ // radiance
0.01130152f, 0.008671843f, 0.02004792f, 0.02515879f, 0.0202014f, 0.02117353f
}
},
{ // turbidity 2
{ // coefficient A
-1.099352f, -1.140694f, -0.9861499f, -1.233397f, -0.9438092f, -1.120794f
},
{ // coefficient B
-0.1338612f, -0.172389f, -0.08032931f, -0.09896833f, -0.1629239f, -0.2121477f
},
{ // coefficient C
-0.8419373f, 1.413194f, 2.396682f, -1.679568f, 4.213446f, 1.326767f
},
{ // coefficient D
2.710346f, 0.3429051f, 0.7281382f, 1.542653f, -0.3419684f, 0.2317528f
},
{ // coefficient E
-0.2241044f, -0.4431396f, 0.1472495f, -9.977726f, 2.110121f, 0.5648291f
},
{ // coefficient F
1.645463f, 0.9757402f, 3.316479f, -0.6579124f, 3.552183f, 0.4155089f
},
{ // coefficient G
0.07621625f, 0.1248306f, -0.1028439f, 0.6386315f, -0.2817116f, 0.3048162f
},
{ // coefficient H
3.548772f, 1.780103f, 3.729469f, 6.157752f, -3.323718f, 0.3186797f
},
{ // coefficient I
0.7268231f, 0.6637254f, 0.9059004f, 0.3985939f, 0.8785077f, 0.6285665f
},
{ // radiance
0.01224368f, 0.00883948f, 0.02189489f, 0.04083873f, 0.01919064f, 0.02455945f
}
},
{ // turbidity 3
{ // coefficient A
-1.117191f, -1.13095f, -1.103903f, -1.107818f, -1.012153f, -1.083766f
},
{ // coefficient B
-0.1510453f, -0.1622534f, -0.1810472f, -0.02407723f, -0.1911658f, -0.1889743f
},
{ // coefficient C
-0.3511136f, 1.197076f, 1.254237f, -0.7518594f, 2.357463f, 1.669813f
},
{ // coefficient D
1.706946f, -1.342532f, 3.367565f, 7.11583f, -2.39272f, 1.20514f
},
{ // coefficient E
-0.382046f, 0.06608991f, -1.746589f, -16.03375f, 7.080581f, -2.973102f
},
{ // coefficient F
1.228101f, -0.1955817f, 4.464939f, -4.077248f, 6.002768f, -1.420288f
},
{ // coefficient G
0.1641555f, 0.3515286f, -0.2831757f, 1.210904f, -0.5120158f, 0.5838643f
},
{ // coefficient H
3.663544f, 3.237403f, -0.2424241f, 6.281392f, -2.039879f, 0.1338574f
},
{ // coefficient I
0.6846104f, 0.5732364f, 0.9961252f, 0.2496837f, 1.008071f, 0.5902153f
},
{ // radiance
0.01252249f, 0.008009335f, 0.02371432f, 0.06397292f, 0.02042384f, 0.02862112f
}
},
{ // turbidity 4
{ // coefficient A
-1.17639f, -1.132876f, -1.296062f, -0.9026255f, -1.103623f, -1.04937f
},
{ // coefficient B
-0.2045914f, -0.1921539f, -0.2855719f, 0.02670242f, -0.1818983f, -0.1854814f
},
{ // coefficient C
-1.035177f, 0.9661015f, 1.143329f, -0.8632572f, 1.490037f, 1.385509f
},
{ // coefficient D
1.521425f, -0.2470184f, -1.640682f, 7.076787f, -3.276017f, 1.714977f
},
{ // coefficient E
-0.09698257f, -0.5825797f, 1.921828f, -14.07525f, 6.936153f, -3.205204f
},
{ // coefficient F
0.7561111f, 0.5044517f, 1.625086f, -1.251814f, 2.689183f, -0.116142f
},
{ // coefficient G
0.2338582f, 0.1418233f, 0.5248871f, 0.1806495f, 0.5705211f, 0.345332f
},
{ // coefficient H
3.2645f, 4.416172f, -3.757434f, 6.575978f, -1.354153f, 0.4355508f
},
{ // coefficient I
0.6520267f, 0.658057f, 0.7293814f, 0.6367149f, 0.709523f, 0.7115006f
},
{ // radiance
0.01180325f, 0.008958479f, 0.0118394f, 0.1148189f, 0.01354571f, 0.03848691f
}
},
{ // turbidity 5
{ // coefficient A
-1.225291f, -1.318856f, -1.168038f, -0.9862998f, -1.049025f, -1.069033f
},
{ // coefficient B
-0.2574996f, -0.3713082f, -0.1572586f, -0.07482649f, -0.140428f, -0.2091865f
},
{ // coefficient C
-0.4766919f, -0.4987985f, 1.786834f, -1.332445f, 1.342894f, 1.08828f
},
{ // coefficient D
0.4951357f, 2.026237f, -4.594005f, 13.2926f, -5.633938f, 3.125506f
},
{ // coefficient E
0.05242961f, -1.457426f, 4.7806f, -18.03019f, 8.396123f, -4.66458f
},
{ // coefficient F
0.4702062f, 0.7809557f, 1.639416f, -2.669936f, 3.809869f, -1.191484f
},
{ // coefficient G
0.2428653f, 0.001419197f, 0.5324063f, 0.3591475f, 0.4313231f, 0.5195344f
},
{ // coefficient H
3.095075f, 2.776845f, -1.000719f, 3.852972f, -0.5373938f, 0.3961673f
},
{ // coefficient I
0.6424221f, 0.7058454f, 0.7325083f, 0.5714399f, 0.7693298f, 0.6817393f
},
{ // radiance
0.01085725f, 0.01134459f, -0.007554548f, 0.1671603f, 0.008431212f, 0.04772824f
}
},
{ // turbidity 6
{ // coefficient A
-1.267895f, -1.551925f, -0.9782795f, -1.077598f, -1.025969f, -1.062382f
},
{ // coefficient B
-0.3004228f, -0.591782f, 0.03652455f, -0.2015253f, -0.1170376f, -0.1881842f
},
{ // coefficient C
-0.5149611f, -0.5915642f, 1.331304f, -1.037818f, 1.139546f, 0.8863153f
},
{ // coefficient D
0.4302967f, 2.016019f, -4.804135f, 13.07045f, -3.37468f, 4.266858f
},
{ // coefficient E
0.1504018f, -1.252286f, 5.130801f, -17.15211f, 6.330035f, -5.011692f
},
{ // coefficient F
0.3700643f, 0.8651065f, 1.052586f, -1.607252f, 2.199093f, -0.408683f
},
{ // coefficient G
0.2354209f, 0.007721402f, 0.4439877f, 0.4472382f, 0.3928627f, 0.5889147f
},
{ // coefficient H
2.887788f, 1.148828f, 2.100157f, 0.6802371f, 0.8428666f, 0.5341939f
},
{ // coefficient I
0.6298388f, 0.6589887f, 0.8308236f, 0.4848934f, 0.796091f, 0.6866073f
},
{ // radiance
0.01014547f, 0.01403737f, -0.02479898f, 0.2032831f, 0.005127749f, 0.05409735f
}
},
{ // turbidity 7
{ // coefficient A
-1.352417f, -1.976438f, -0.7197412f, -1.189984f, -0.9781383f, -1.081242f
},
{ // coefficient B
-0.3836953f, -0.9152777f, 0.216521f, -0.3151612f, -0.08764833f, -0.2051949f
},
{ // coefficient C
-0.595156f, -0.4417666f, 0.6233215f, -0.5020073f, 0.8290915f, 0.6056482f
},
{ // coefficient D
0.4053297f, 1.790189f, -3.960686f, 10.15136f, 0.108696f, 5.219268f
},
{ // coefficient E
0.2468304f, -1.085567f, 4.694083f, -14.83684f, 4.46757f, -4.410608f
},
{ // coefficient F
0.2565711f, 0.4562228f, 2.062797f, -3.47673f, 3.905633f, -1.963212f
},
{ // coefficient G
0.2321365f, 0.02133332f, 0.3121864f, 0.6049863f, 0.1975895f, 0.7277904f
},
{ // coefficient H
2.512376f, 0.4989856f, 2.942681f, -0.1941153f, 0.6964295f, 0.8509624f
},
{ // coefficient I
0.6089904f, 0.62571f, 0.8846033f, 0.4281015f, 0.8333166f, 0.6689251f
},
{ // radiance
0.009143208f, 0.01825961f, -0.04902126f, 0.2473204f, 0.007992708f, 0.06238918f
}
},
{ // turbidity 8
{ // coefficient A
-1.443212f, -3.021428f, -0.1593477f, -1.378374f, -0.9436176f, -1.078915f
},
{ // coefficient B
-0.4725757f, -1.558459f, 0.5076305f, -0.5064534f, -0.06189764f, -0.2051854f
},
{ // coefficient C
-0.9477961f, 0.1389763f, -0.7283933f, 0.7231197f, 0.1144482f, 0.4754209f
},
{ // coefficient D
0.2671049f, 1.122031f, -1.265076f, 1.385088f, 9.137694f, 0.005826389f
},
{ // coefficient E
0.6163577f, -1.014816f, 3.904153f, -9.500118f, 0.2948867f, -2.498632f
},
{ // coefficient F
-0.3965036f, 1.265687f, -0.08196757f, -0.4806746f, 1.224655f, -0.8265078f
},
{ // coefficient G
0.4071134f, -0.1649604f, 0.6585734f, 0.1067642f, 0.3675148f, 0.764467f
},
{ // coefficient H
2.464218f, -0.6627923f, 4.998464f, -3.297209f, 2.12409f, 0.6066256f
},
{ // coefficient I
0.499782f, 0.5494665f, 0.9330667f, 0.4441955f, 0.8014519f, 0.6811963f
},
{ // radiance
0.007784746f, 0.02352715f, -0.07647231f, 0.2868914f, 0.02581232f, 0.07345921f
}
},
{ // turbidity 9
{ // coefficient A
-1.621711f, -4.003597f, -0.7696712f, -0.8506589f, -1.150061f, -1.020853f
},
{ // coefficient B
-0.6425839f, -1.736865f, -0.3872306f, -0.05430791f, -0.3009215f, -0.1499849f
},
{ // coefficient C
-0.7307813f, -0.606245f, -1.159436f, 1.088719f, -0.09176868f, 0.3437744f
},
{ // coefficient D
1.109519f, -0.7370264f, 1.660752f, -2.513532f, 8.996074f, -0.002339597f
},
{ // coefficient E
-0.4107299f, 0.7002778f, 2.03184f, -4.144477f, -1.753862f, -2.64652f
},
{ // coefficient F
0.7789953f, -0.4140325f, -0.7697685f, 2.760914f, -2.428423f, 1.378457f
},
{ // coefficient G
0.07764812f, 0.3926087f, 0.5481684f, -0.3011272f, 0.5764492f, 0.670841f
},
{ // coefficient H
2.335627f, 0.8316399f, 2.788249f, -2.383556f, 1.218492f, 0.914968f
},
{ // coefficient I
0.6230915f, 0.1516341f, 1.112977f, 0.449607f, 0.755223f, 0.6961806f
},
{ // radiance
0.006425568f, 0.02680506f, -0.09216963f, 0.2866525f, 0.07712816f, 0.08877708f
}
},
{ // turbidity 10
{ // coefficient A
-1.982394f, -3.432415f, -4.333533f, 0.4889224f, -1.347508f, -1.047037f
},
{ // coefficient B
-0.9369621f, -1.264283f, -2.156157f, 0.05956721f, -0.1874523f, -0.3743109f
},
{ // coefficient C
-0.3767595f, -0.9042363f, -0.8341654f, 0.8445388f, -0.05063342f, 0.3452375f
},
{ // coefficient D
2.514582f, -2.930432f, 5.048387f, -7.479177f, 12.99813f, -0.003615559f
},
{ // coefficient E
-1.501415f, 0.8249096f, 1.80358f, -4.049292f, -1.707387f, -2.169432f
},
{ // coefficient F
0.6258952f, 0.724484f, -0.9318434f, 1.199966f, -0.4076088f, -0.0716688f
},
{ // coefficient G
-5.26043e-06f, 0.03055253f, 1.085401f, -0.706056f, 0.6119042f, 0.3747046f
},
{ // coefficient H
2.100142f, 2.527405f, 0.872833f, -0.9742353f, 0.371779f, 0.2746126f
},
{ // coefficient I
0.7824077f, -0.002107031f, 1.050061f, 0.4893674f, 0.7662157f, 0.6773818f
},
{ // radiance
0.005348459f, 0.02347534f, -0.07499904f, 0.2035369f, 0.1874982f, 0.1045961f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.105396f, -1.13248f, -1.036155f, -1.162485f, -1.031787f, -1.081125f
},
{ // coefficient B
-0.1663441f, -0.2330883f, -0.0943245f, -0.1382303f, -0.2165067f, -0.1623953f
},
{ // coefficient C
-0.2359634f, 2.273938f, 3.117262f, 0.1544122f, 4.446666f, 2.048137f
},
{ // coefficient D
2.476496f, 0.2422717f, -0.6369808f, 0.3716511f, -0.1470711f, 0.08639386f
},
{ // coefficient E
-0.1329052f, -0.4159883f, 1.060389f, -9.520209f, 2.143436f, 1.225845f
},
{ // coefficient F
1.715759f, 0.9350714f, 3.386481f, -0.2739605f, 1.922123f, 0.1239255f
},
{ // coefficient G
0.002103991f, 0.01432347f, -0.04605081f, 0.08489054f, -0.0958903f, 0.08366038f
},
{ // coefficient H
1.421966f, -0.4176684f, 1.248f, 4.37157f, -3.594664f, 0.8322625f
},
{ // coefficient I
0.6270733f, 0.466464f, 0.9701974f, 0.07694391f, 0.8629249f, 0.5604708f
},
{ // radiance
0.01168435f, 0.009445039f, 0.0195052f, 0.02563333f, 0.04729214f, 0.05096829f
}
},
{ // turbidity 2
{ // coefficient A
-1.112606f, -1.09863f, -1.077575f, -1.099714f, -1.061945f, -1.087782f
},
{ // coefficient B
-0.1514491f, -0.1410161f, -0.1275645f, -0.04474282f, -0.2200148f, -0.1709041f
},
{ // coefficient C
-1.608942f, 2.155487f, 1.418205f, -0.1247041f, 4.333103f, 1.834683f
},
{ // coefficient D
3.592344f, -0.438161f, 2.057232f, -0.1578342f, 0.01818948f, 0.07351214f
},
{ // coefficient E
-0.1616115f, -0.3635146f, -0.07778503f, -7.968895f, 1.907751f, 1.282669f
},
{ // coefficient F
1.593861f, 1.313843f, 2.43384f, 0.1730912f, 1.560633f, 0.2075884f
},
{ // coefficient G
0.05488306f, 0.1201277f, -0.07094034f, 0.5303765f, -0.3182192f, 0.2698971f
},
{ // coefficient H
2.854783f, 2.469958f, 3.347259f, 5.523179f, -3.010282f, 0.3373859f
},
{ // coefficient I
0.7519999f, 0.6481071f, 0.9344752f, 0.384034f, 0.8844351f, 0.5808336f
},
{ // radiance
0.01252521f, 0.0101294f, 0.02002302f, 0.04276714f, 0.04778275f, 0.05748908f
}
},
{ // turbidity 3
{ // coefficient A
-1.121417f, -1.11891f, -1.121145f, -1.078889f, -1.053346f, -1.100093f
},
{ // coefficient B
-0.155122f, -0.1496848f, -0.1980292f, -0.001541161f, -0.2262166f, -0.1810893f
},
{ // coefficient C
-1.527123f, 2.736873f, -0.5407002f, 1.37065f, 2.532003f, 2.56643f
},
{ // coefficient D
2.788105f, -1.829596f, 2.886807f, 1.64445f, -0.8720058f, 0.47577f
},
{ // coefficient E
-0.1477847f, -0.3411991f, 0.4877852f, -14.25572f, 7.667901f, -2.069684f
},
{ // coefficient F
1.152139f, 0.1177538f, 3.692424f, -3.07543f, 4.357082f, -1.585209f
},
{ // coefficient G
0.1742181f, 0.2960683f, -0.1104033f, 0.9346385f, -0.5367969f, 0.469314f
},
{ // coefficient H
3.473417f, 3.273898f, -0.2358878f, 5.871863f, -2.073329f, -0.6719615f
},
{ // coefficient I
0.673705f, 0.5743833f, 0.9976295f, 0.2618026f, 1.023965f, 0.5569453f
},
{ // radiance
0.01285682f, 0.009705956f, 0.02018767f, 0.06797612f, 0.05181662f, 0.06548541f
}
},
{ // turbidity 4
{ // coefficient A
-1.181528f, -1.142132f, -1.225408f, -0.9815988f, -1.103439f, -1.081695f
},
{ // coefficient B
-0.2151896f, -0.1825403f, -0.2906477f, 0.05577004f, -0.2481553f, -0.1943361f
},
{ // coefficient C
-1.07647f, 1.320992f, 0.8323962f, -0.4240348f, 3.045691f, 2.303236f
},
{ // coefficient D
2.037517f, -1.895472f, 2.1763f, 1.488466f, -0.6993993f, 0.3906537f
},
{ // coefficient E
-0.1683559f, -0.2160241f, 1.943859f, -12.79361f, 6.081464f, -1.498982f
},
{ // coefficient F
0.878011f, -0.2079637f, 3.003671f, -3.110899f, 3.986228f, -1.37214f
},
{ // coefficient G
0.1782773f, 0.3424953f, 0.1130255f, 0.7360104f, -0.1616828f, 0.3379214f
},
{ // coefficient H
2.76865f, 4.333355f, -4.449977f, 7.937751f, -3.373973f, -0.4953601f
},
{ // coefficient I
0.6743293f, 0.566342f, 0.886739f, 0.4474817f, 0.8572294f, 0.6521383f
},
{ // radiance
0.0121897f, 0.01070161f, 0.008436124f, 0.1177409f, 0.05124773f, 0.08069723f
}
},
{ // turbidity 5
{ // coefficient A
-1.235776f, -1.268478f, -1.190389f, -0.96323f, -1.119689f, -1.078742f
},
{ // coefficient B
-0.2745978f, -0.3078367f, -0.2597641f, 0.07067379f, -0.3019499f, -0.184088f
},
{ // coefficient C
0.02205316f, -0.2041769f, 1.377486f, -0.6690546f, 2.657474f, 2.341448f
},
{ // coefficient D
0.4505193f, 1.189583f, -2.289003f, 9.507059f, -5.208105f, 2.576821f
},
{ // coefficient E
-0.1569202f, -1.311782f, 4.214767f, -17.21426f, 10.152f, -5.011296f
},
{ // coefficient F
0.5065039f, 0.4724677f, 1.900318f, -2.850631f, 3.863887f, -1.671996f
},
{ // coefficient G
0.201632f, 0.1025764f, 0.3126604f, 0.6931159f, -0.1603721f, 0.3955131f
},
{ // coefficient H
2.449382f, 3.300615f, -2.537399f, 5.722238f, -3.132328f, -0.4700684f
},
{ // coefficient I
0.6535188f, 0.6689797f, 0.7980811f, 0.4856193f, 0.8543154f, 0.6516593f
},
{ // radiance
0.01129406f, 0.01339498f, -0.01193989f, 0.1709051f, 0.05001995f, 0.09558448f
}
},
{ // turbidity 6
{ // coefficient A
-1.263449f, -1.544305f, -0.9436041f, -1.106718f, -1.048783f, -1.10814f
},
{ // coefficient B
-0.3016185f, -0.5656222f, -0.01620107f, -0.1102586f, -0.2156089f, -0.2156079f
},
{ // coefficient C
-0.1680842f, -0.2084263f, 1.253874f, -0.8803979f, 2.786728f, 2.181504f
},
{ // coefficient D
0.4791728f, 1.312304f, -3.49912f, 13.26904f, -4.862603f, 2.124553f
},
{ // coefficient E
-0.08989862f, -0.8580598f, 4.452594f, -18.78253f, 8.438175f, -5.007442f
},
{ // coefficient F
0.5069014f, 0.4039133f, 1.87049f, -2.994671f, 3.683031f, -1.711417f
},
{ // coefficient G
0.189281f, 0.1008588f, 0.152471f, 0.9501205f, -0.3092444f, 0.4904871f
},
{ // coefficient H
2.535339f, 1.268784f, 0.8031346f, 2.284893f, -1.293934f, -0.8710312f
},
{ // coefficient I
0.6506411f, 0.6333075f, 0.8930942f, 0.3662824f, 0.9110045f, 0.6376965f
},
{ // radiance
0.01056724f, 0.01570995f, -0.02759057f, 0.203076f, 0.05311962f, 0.1037882f
}
},
{ // turbidity 7
{ // coefficient A
-1.345282f, -1.907309f, -0.7540737f, -1.165479f, -1.032259f, -1.119906f
},
{ // coefficient B
-0.3856197f, -0.8881952f, 0.1800167f, -0.2500464f, -0.1593662f, -0.2533313f
},
{ // coefficient C
-0.1975094f, -0.482786f, 1.174575f, -0.9603937f, 2.621639f, 2.017975f
},
{ // coefficient D
0.3016919f, 1.495665f, -3.052295f, 8.919733f, -0.5369281f, 0.4776492f
},
{ // coefficient E
0.2765844f, -0.9243173f, 4.66906f, -15.61899f, 6.011517f, -5.005474f
},
{ // coefficient F
0.2092275f, 0.3864061f, 1.793876f, -2.85065f, 3.234312f, -1.503414f
},
{ // coefficient G
0.2251325f, 0.07491538f, 0.1664066f, 0.8368531f, -0.1842335f, 0.4694448f
},
{ // coefficient H
2.058589f, 0.2719526f, 2.376187f, 0.3404393f, -0.3749684f, -1.0392f
},
{ // coefficient I
0.6014266f, 0.6046039f, 0.9261122f, 0.3694265f, 0.8913063f, 0.643863f
},
{ // radiance
0.009594077f, 0.01966415f, -0.05024827f, 0.2432607f, 0.06172355f, 0.1169276f
}
},
{ // turbidity 8
{ // coefficient A
-1.457842f, -2.9066f, -0.2500868f, -1.28224f, -1.046065f, -1.104124f
},
{ // coefficient B
-0.4806585f, -1.55847f, 0.5175467f, -0.4792753f, -0.1097023f, -0.2576744f
},
{ // coefficient C
-0.6807833f, 0.4048869f, -0.436602f, 0.3824491f, 1.672284f, 1.91903f
},
{ // coefficient D
0.268252f, 1.012321f, -1.156645f, 2.061672f, 4.799701f, -0.001296687f
},
{ // coefficient E
0.5586195f, -0.8709928f, 3.657335f, -9.474747f, 1.286403f, -3.500874f
},
{ // coefficient F
-0.2806536f, 0.9025637f, 0.4092973f, -1.080446f, 1.712569f, -1.048831f
},
{ // coefficient G
0.3694491f, -0.09171486f, 0.4611188f, 0.38521f, 0.1099301f, 0.39294f
},
{ // coefficient H
2.169304f, -1.279805f, 4.971048f, -3.234591f, 1.214451f, -0.9229794f
},
{ // coefficient I
0.4997873f, 0.5327924f, 0.9789196f, 0.3696703f, 0.8572886f, 0.6687909f
},
{ // radiance
0.008348105f, 0.02415281f, -0.07475064f, 0.2748209f, 0.08753187f, 0.1356651f
}
},
{ // turbidity 9
{ // coefficient A
-1.658918f, -3.553256f, -1.299367f, -0.4366852f, -1.334075f, -1.05246f
},
{ // coefficient B
-0.6637256f, -1.639022f, -0.4890457f, -0.02303554f, -0.2466639f, -0.2501759f
},
{ // coefficient C
-1.16285f, -0.8821743f, -0.5595307f, 0.541753f, 1.293237f, 1.601487f
},
{ // coefficient D
1.829549f, -0.2660037f, 1.129115f, -0.91069f, 4.685982f, 1.713504f
},
{ // coefficient E
-0.2525065f, 0.5168738f, 0.5445041f, -0.4398484f, -4.032879f, -2.810006f
},
{ // coefficient F
0.8624803f, -0.858277f, 0.5361314f, 0.3178252f, 0.4666293f, -0.8042597f
},
{ // coefficient G
0.05214746f, 0.411078f, 0.4579589f, -0.2132156f, 0.5337822f, 0.3216101f
},
{ // coefficient H
1.977784f, 0.1034123f, 3.385708f, -3.682761f, 1.60329f, -0.5944876f
},
{ // coefficient I
0.6455044f, 0.1039852f, 1.184267f, 0.375447f, 0.80674f, 0.6740783f
},
{ // radiance
0.007106169f, 0.02513833f, -0.07928946f, 0.2463668f, 0.1685984f, 0.148582f
}
},
{ // turbidity 10
{ // coefficient A
-1.974746f, -2.677119f, -5.323827f, 1.325f, -1.741196f, -0.9762671f
},
{ // coefficient B
-0.911191f, -1.100702f, -2.46105f, 0.3538115f, -0.3448017f, -0.3368355f
},
{ // coefficient C
-0.2159155f, -0.6847246f, -0.3941307f, 0.1783573f, 1.242377f, 1.374728f
},
{ // coefficient D
2.437415f, -2.820855f, 4.908417f, -6.90365f, 10.98613f, -0.003312353f
},
{ // coefficient E
-1.592257f, 1.577837f, -0.1657232f, -0.8079921f, -3.386782f, -2.576919f
},
{ // coefficient F
0.6133797f, 0.6243833f, -0.9501763f, 1.215973f, -0.3128683f, -0.1778015f
},
{ // coefficient G
-6.243023e-06f, 0.02218481f, 1.06352f, -0.685254f, 0.5792848f, 0.2024288f
},
{ // coefficient H
1.960399f, 1.568717f, 1.455622f, -1.735835f, 0.0136433f, -0.6128666f
},
{ // coefficient I
0.7806415f, 0.02327673f, 0.978189f, 0.5744662f, 0.7112763f, 0.6917938f
},
{ // radiance
0.00603161f, 0.02016534f, -0.05448075f, 0.1436689f, 0.297183f, 0.1639884f
}
}
}
}
};

const float datasetsRGB[3][2][10][10][6] = {
{ // Red
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.099459f, -1.169858f, -0.9665225f, -1.292508f, -0.9264666f, -1.139072f
},
{ // coefficient B
-0.1335146f, -0.1832793f, -0.1403888f, -0.1299552f, -0.169678f, -0.1796056f
},
{ // coefficient C
-4.083223f, 0.9694744f, 5.194457f, -2.071404f, 4.57407f, 1.923311f
},
{ // coefficient D
5.919603f, 0.09495762f, -1.107607f, -0.04752482f, -0.4232936f, 6.788529f
},
{ // coefficient E
-0.1104166f, -0.04738918f, -0.8135181f, 1.215598f, -7.575833f, -2.364389f
},
{ // coefficient F
1.600158f, 0.2194171f, 4.969661f, -1.904179f, 5.079755f, -1.064041f
},
{ // coefficient G
-1.326538e-06f, 0.1095749f, -0.2300508f, 0.3027985f, -0.2576343f, 0.171701f
},
{ // coefficient H
4.917807f, 3.603604f, -2.48935f, 8.707768f, -4.506805f, 1.534681f
},
{ // coefficient I
0.5127716f, 0.3815119f, 1.279158f, 0.06332446f, 0.6908129f, 0.501581f
},
{ // radiance
1.962684f, 1.159831f, 4.450588f, 5.079633f, 4.437388f, 4.324573f
}
},
{ // turbidity 2
{ // coefficient A
-1.107257f, -1.187337f, -0.9685321f, -1.252817f, -0.9459509f, -1.125092f
},
{ // coefficient B
-0.1384411f, -0.1969013f, -0.1553308f, -0.05129949f, -0.2322133f, -0.179675f
},
{ // coefficient C
-4.285744f, 0.8551048f, 4.732492f, -2.800433f, 4.375041f, 1.626399f
},
{ // coefficient D
5.713157f, 0.05289708f, -1.178935f, -0.01295992f, -0.1712018f, 6.989743f
},
{ // coefficient E
-0.1015992f, -0.07626406f, -0.7852791f, 1.308964f, -7.451681f, -2.406382f
},
{ // coefficient F
1.372638f, 0.01733153f, 4.604492f, -2.204331f, 5.078019f, -0.9060383f
},
{ // coefficient G
0.06555893f, 0.1779454f, -0.2666518f, 0.7276011f, -0.4223538f, 0.2961611f
},
{ // coefficient H
5.127514f, 3.801038f, -2.367663f, 8.699265f, -4.595561f, 1.337715f
},
{ // coefficient I
0.6550471f, 0.4742709f, 1.177527f, 0.1188388f, 1.074719f, 0.543814f
},
{ // radiance
1.946487f, 1.287515f, 3.703696f, 8.782833f, 3.440437f, 5.160333f
}
},
{ // turbidity 3
{ // coefficient A
-1.135338f, -1.13278f, -1.167176f, -1.042529f, -1.061598f, -1.07356f
},
{ // coefficient B
-0.171616f, -0.1456214f, -0.2927225f, 0.04110823f, -0.2096143f, -0.2077964f
},
{ // coefficient C
-1.499253f, -1.736672f, 5.727667f, -4.000629f, 3.803155f, 1.492052f
},
{ // coefficient D
2.373491f, 1.756589f, -3.139244f, 4.362364f, -7.977069f, 16.26322f
},
{ // coefficient E
-0.1654023f, -0.1087003f, -0.6425204f, 1.09054f, -3.63788f, -5.015304f
},
{ // coefficient F
0.9566404f, 0.3757927f, 2.822634f, -1.338674f, 3.707671f, -0.4059889f
},
{ // coefficient G
0.1113453f, 0.252507f, -0.1457812f, 0.8246964f, -0.1903128f, 0.2659782f
},
{ // coefficient H
4.528473f, 7.178513f, -6.78708f, 10.95249f, -3.397953f, 0.639538f
},
{ // coefficient I
0.6579439f, 0.5003814f, 1.017072f, 0.2912211f, 0.99715f, 0.5634436f
},
{ // radiance
1.88217f, 1.335878f, 2.648641f, 13.58368f, 3.105473f, 5.907387f
}
},
{ // turbidity 4
{ // coefficient A
-1.172794f, -1.238374f, -1.176923f, -1.003284f, -1.060017f, -1.081141f
},
{ // coefficient B
-0.2111186f, -0.2670827f, -0.2574586f, 0.01943984f, -0.2037206f, -0.2123302f
},
{ // coefficient C
-1.360013f, 0.3247678f, 2.304045f, -2.145066f, 2.483018f, 1.092275f
},
{ // coefficient D
1.60408f, 0.5466311f, -2.797678f, 10.30924f, -4.595459f, 2.683841f
},
{ // coefficient E
-0.08473723f, -0.7425952f, 1.464405f, -15.25413f, 6.526991f, -4.166938f
},
{ // coefficient F
0.7217312f, 0.527644f, 1.998552f, -2.02301f, 4.031804f, -1.396582f
},
{ // coefficient G
0.154803f, 0.02678026f, 0.2550559f, 0.5448699f, 0.1206513f, 0.4371205f
},
{ // coefficient H
4.25701f, 5.484169f, -4.199772f, 8.159497f, -2.586527f, 1.030233f
},
{ // coefficient I
0.6328974f, 0.6814734f, 0.7544892f, 0.5539148f, 0.7875752f, 0.6664862f
},
{ // radiance
1.738159f, 1.624289f, -0.008786695f, 21.18253f, 2.770255f, 7.055672f
}
},
{ // turbidity 5
{ // coefficient A
-1.222392f, -1.42461f, -1.130655f, -0.9739015f, -1.082178f, -1.063354f
},
{ // coefficient B
-0.2651924f, -0.4710155f, -0.1358609f, -0.06674749f, -0.1974495f, -0.1727108f
},
{ // coefficient C
-0.4625037f, -0.1826815f, 0.9171203f, -0.4768897f, 1.050245f, 0.9681592f
},
{ // coefficient D
0.3521964f, 1.786277f, -4.660394f, 12.48589f, -4.792855f, 2.736077f
},
{ // coefficient E
0.02148855f, -1.952442f, 6.251162f, -19.94688f, 8.663406f, -4.969269f
},
{ // coefficient F
0.5078494f, 0.5277612f, 1.904529f, -2.353043f, 3.246969f, -0.836057f
},
{ // coefficient G
0.179159f, -0.01773629f, 0.2639668f, 0.5885575f, 0.1556731f, 0.5994612f
},
{ // coefficient H
3.852516f, 2.415874f, 1.85613f, 1.287251f, 0.8117442f, 1.024039f
},
{ // coefficient I
0.5998216f, 0.6701272f, 0.822844f, 0.4830135f, 0.8050376f, 0.6786935f
},
{ // radiance
1.571896f, 2.301786f, -4.028545f, 29.66806f, 1.630876f, 8.711031f
}
},
{ // turbidity 6
{ // coefficient A
-1.261936f, -1.681088f, -0.9453001f, -1.086609f, -1.041259f, -1.063473f
},
{ // coefficient B
-0.3053676f, -0.6971919f, 0.03460388f, -0.2029011f, -0.138879f, -0.1916051f
},
{ // coefficient C
-0.4262222f, -0.1105652f, 0.6067038f, -0.639917f, 1.147331f, 0.6556979f
},
{ // coefficient D
0.4000196f, 0.7437426f, -1.985128f, 6.926885f, 6.282086f, -0.003371891f
},
{ // coefficient E
-0.02059388f, -0.6594399f, 3.457236f, -15.12189f, 3.679836f, -3.699664f
},
{ // coefficient F
0.4721802f, 0.2254221f, 2.655483f, -3.793051f, 4.398314f, -1.926783f
},
{ // coefficient G
0.1480028f, 0.08710195f, -0.01162354f, 0.945612f, -0.1355232f, 0.7371154f
},
{ // coefficient H
3.505343f, 1.263913f, 3.304716f, 0.2222222f, 1.031134f, 1.179975f
},
{ // coefficient I
0.6121337f, 0.5681865f, 1.00195f, 0.2893725f, 0.9273509f, 0.6367068f
},
{ // radiance
1.475048f, 2.679086f, -6.311315f, 33.77896f, 2.140975f, 9.385283f
}
},
{ // turbidity 7
{ // coefficient A
-1.33639f, -2.119878f, -0.7761432f, -1.118734f, -1.026437f, -1.069949f
},
{ // coefficient B
-0.3778927f, -1.055472f, 0.2124336f, -0.3513815f, -0.08257946f, -0.2029607f
},
{ // coefficient C
-0.7259477f, 0.5422052f, -0.6845184f, 0.5500918f, 0.3221701f, 0.5825042f
},
{ // coefficient D
0.2270247f, 0.7826648f, -0.9812342f, 0.9449627f, 11.98372f, -0.002398595f
},
{ // coefficient E
0.4627513f, -1.286065f, 4.347257f, -12.6207f, 1.55513f, -3.278335f
},
{ // coefficient F
0.1366459f, 0.9517905f, 0.967198f, -1.82528f, 2.560304f, -1.349882f
},
{ // coefficient G
0.2637347f, -0.1432358f, 0.377315f, 0.473126f, 0.1406465f, 0.7208433f
},
{ // coefficient H
3.292059f, -0.2379816f, 5.789529f, -3.326892f, 2.912858f, 0.8505164f
},
{ // coefficient I
0.4998211f, 0.5910513f, 0.9646598f, 0.3568768f, 0.8643181f, 0.6625391f
},
{ // radiance
1.326174f, 3.378759f, -9.831444f, 39.42061f, 2.852702f, 10.82542f
}
},
{ // turbidity 8
{ // coefficient A
-1.392309f, -2.713405f, -0.9588644f, -0.8028546f, -1.198326f, -1.013531f
},
{ // coefficient B
-0.4454945f, -1.395112f, 0.024339f, -0.2473563f, -0.1804865f, -0.185906f
},
{ // coefficient C
-0.5664f, 0.202923f, -1.527493f, 1.617898f, -0.3565414f, 0.5799857f
},
{ // coefficient D
0.6283393f, 0.1877272f, -0.9632874f, 2.073591f, 4.0732f, 13.31883f
},
{ // coefficient E
-0.3761727f, -0.3715859f, 5.496269f, -11.49446f, 1.662086f, -4.346873f
},
{ // coefficient F
0.6949802f, -0.1652929f, 1.094931f, -0.8394131f, 1.23977f, -1.11382f
},
{ // coefficient G
0.07748178f, 0.2385861f, 0.2004044f, 0.2726847f, 0.3367978f, 0.5275714f
},
{ // coefficient H
3.192797f, -0.4150768f, 6.084554f, -4.634538f, 2.997402f, 0.8045177f
},
{ // coefficient I
0.5968661f, 0.1375467f, 1.369604f, 0.1367293f, 0.9360383f, 0.6496373f
},
{ // radiance
1.153344f, 3.967771f, -12.65181f, 41.95016f, 7.468239f, 12.2135f
}
},
{ // turbidity 9
{ // coefficient A
-1.530103f, -2.415469f, -3.096796f, 0.2713904f, -1.45658f, -0.9860389f
},
{ // coefficient B
-0.6107468f, -1.057499f, -1.465688f, 0.2852852f, -0.345596f, -0.2099564f
},
{ // coefficient C
-0.3841771f, -0.4161968f, -1.199232f, 1.20209f, -0.06409257f, 0.294665f
},
{ // coefficient D
1.881508f, -2.357548f, 4.567061f, -8.206784f, 16.67697f, -0.0035478f
},
{ // coefficient E
-1.464807f, 0.6300296f, 3.26098f, -5.805762f, -2.311094f, -2.268313f
},
{ // coefficient F
0.665469f, 0.6224915f, -0.9794907f, 1.804431f, -0.9771104f, -0.06205647f
},
{ // coefficient G
-5.950797e-06f, 0.01545048f, 0.8950491f, -0.6090648f, 0.6759863f, 0.4705185f
},
{ // coefficient H
2.738912f, 2.038561f, 2.049235f, -1.990902f, 1.245136f, 0.8657995f
},
{ // coefficient I
0.8101012f, -0.1339415f, 1.331015f, 0.3288858f, 0.7911932f, 0.6856284f
},
{ // radiance
0.9746081f, 4.051626f, -12.98454f, 37.54964f, 17.49232f, 14.20619f
}
},
{ // turbidity 10
{ // coefficient A
-1.971736f, -1.589845f, -6.940173f, 1.293144f, -1.48888f, -1.060891f
},
{ // coefficient B
-0.9414047f, -0.7797079f, -2.823963f, -0.001919778f, -0.2495496f, -0.4035829f
},
{ // coefficient C
-0.3400557f, -0.558224f, -1.620848f, 1.644206f, -0.2377137f, 0.2823207f
},
{ // coefficient D
1.468763f, -0.8137376f, 1.090696f, -0.8666967f, 11.67714f, -0.002369798f
},
{ // coefficient E
-1.474284f, 0.5846617f, 2.39173f, -7.161953f, -0.8617124f, -1.876577f
},
{ // coefficient F
0.5501062f, 0.1129459f, 1.370047f, -1.385018f, 1.053828f, -0.5950265f
},
{ // coefficient G
-1.10975e-05f, -0.02658005f, 0.5890462f, -0.1505374f, 0.1992744f, 0.4241017f
},
{ // coefficient H
2.35637f, 2.707248f, 1.7284f, -1.388643f, 0.3633564f, 0.3140802f
},
{ // coefficient I
0.9001702f, -0.2112486f, 1.331253f, 0.2530122f, 0.8553304f, 0.6631669f
},
{ // radiance
0.8448016f, 3.181809f, -8.757338f, 21.97962f, 35.24033f, 16.39549f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.101204f, -1.154597f, -1.031588f, -1.159454f, -1.03594f, -1.096669f
},
{ // coefficient B
-0.1351353f, -0.1923378f, -0.1546804f, -0.09546699f, -0.2021151f, -0.1558196f
},
{ // coefficient C
-4.030882f, 0.8512132f, 5.266214f, -1.508146f, 4.719343f, 2.057553f
},
{ // coefficient D
6.096353f, 0.2934895f, -0.949139f, -0.02031411f, -0.9019318f, 6.274495f
},
{ // coefficient E
-0.1148599f, -0.06522777f, -0.7184867f, 1.040653f, -7.858046f, -2.678352f
},
{ // coefficient F
1.606507f, 0.1389077f, 4.875626f, -2.333508f, 3.901234f, -1.814927f
},
{ // coefficient G
-1.555474e-06f, 0.09091469f, -0.1911907f, 0.2540592f, -0.2233137f, 0.1550676f
},
{ // coefficient H
4.436084f, 3.133307f, -2.865642f, 8.594981f, -4.344739f, 1.903276f
},
{ // coefficient I
0.5973715f, 0.2108541f, 1.087895f, 0.0931677f, 0.6550733f, 0.4998989f
},
{ // radiance
2.029623f, 1.364434f, 4.201529f, 5.415099f, 9.825839f, 10.63328f
}
},
{ // turbidity 2
{ // coefficient A
-1.114209f, -1.149397f, -1.056121f, -1.125047f, -1.051444f, -1.093847f
},
{ // coefficient B
-0.1473531f, -0.1981628f, -0.1456172f, -0.04003662f, -0.2804541f, -0.1436965f
},
{ // coefficient C
-7.602914f, 4.914096f, 0.4272656f, 1.058457f, 3.364668f, 2.38478f
},
{ // coefficient D
8.973685f, -3.498986f, 2.912649f, -3.462236f, 3.293787f, 5.78707f
},
{ // coefficient E
-0.04980074f, -0.0625709f, -0.5501745f, 0.4395278f, -10.15741f, -2.445987f
},
{ // coefficient F
1.289198f, 0.1667401f, 4.406542f, -2.395805f, 3.807407f, -1.311171f
},
{ // coefficient G
0.08366906f, 0.104898f, -0.138768f, 0.5177589f, -0.3592377f, 0.2326563f
},
{ // coefficient H
4.557987f, 2.284689f, 1.245555f, 4.866247f, -3.367415f, 1.158439f
},
{ // coefficient I
0.6118757f, 0.5935965f, 0.9733011f, 0.4253189f, 0.7900825f, 0.5555416f
},
{ // radiance
2.023126f, 1.494728f, 3.420413f, 9.072178f, 9.205157f, 11.86639f
}
},
{ // turbidity 3
{ // coefficient A
-1.134824f, -1.143017f, -1.136801f, -1.063652f, -1.061015f, -1.106302f
},
{ // coefficient B
-0.1680468f, -0.1565926f, -0.2907502f, 0.07808107f, -0.2859814f, -0.1697123f
},
{ // coefficient C
-3.32562f, 0.3014687f, 3.682423f, -1.983678f, 4.223615f, 2.087196f
},
{ // coefficient D
4.458596f, -0.1763027f, -0.4061202f, 0.3648078f, -2.290138f, 8.238929f
},
{ // coefficient E
-0.1135063f, -0.03557925f, -0.8728159f, 2.102276f, -8.31401f, -2.992416f
},
{ // coefficient F
1.1045f, -0.2342406f, 4.00151f, -3.06505f, 4.405718f, -1.821776f
},
{ // coefficient G
0.07794544f, 0.2528705f, -0.1522202f, 0.8431951f, -0.4613627f, 0.3434859f
},
{ // coefficient H
4.609952f, 5.884085f, -5.528713f, 10.3883f, -4.50291f, 0.7755179f
},
{ // coefficient I
0.6854854f, 0.4750602f, 1.044847f, 0.2662834f, 1.008383f, 0.534119f
},
{ // radiance
1.956307f, 1.648665f, 2.039712f, 14.30239f, 9.039526f, 13.30453f
}
},
{ // turbidity 4
{ // coefficient A
-1.17111f, -1.20048f, -1.223587f, -0.9452673f, -1.134681f, -1.083237f
},
{ // coefficient B
-0.2106304f, -0.2235733f, -0.3502622f, 0.1468459f, -0.3310951f, -0.1831394f
},
{ // coefficient C
-1.614361f, 1.01439f, 1.699106f, -1.335034f, 3.571244f, 2.062654f
},
{ // coefficient D
2.378103f, -1.174074f, 0.6724266f, 4.346628f, -2.208948f, 1.385424f
},
{ // coefficient E
-0.1625969f, -0.444018f, 1.268567f, -12.85652f, 6.04158f, -5.00495f
},
{ // coefficient F
0.8504483f, 0.2262406f, 2.135102f, -1.807046f, 3.107577f, -1.332669f
},
{ // coefficient G
0.1059312f, 0.1665868f, 0.0008039374f, 0.8175243f, -0.3112127f, 0.3627352f
},
{ // coefficient H
4.046256f, 5.461829f, -5.221111f, 9.301065f, -4.186351f, 0.332315f
},
{ // coefficient I
0.6618227f, 0.567631f, 0.944569f, 0.3656798f, 0.9188333f, 0.6191181f
},
{ // radiance
1.825053f, 1.985022f, -0.8036307f, 22.02493f, 9.415361f, 15.17659f
}
},
{ // turbidity 5
{ // coefficient A
-1.211527f, -1.433017f, -1.08492f, -1.008242f, -1.103151f, -1.102235f
},
{ // coefficient B
-0.2590617f, -0.4733592f, -0.1587903f, 0.00279303f, -0.2799598f, -0.2025061f
},
{ // coefficient C
-0.1660874f, 0.1724445f, 0.8999585f, -0.3507469f, 2.525791f, 2.08566f
},
{ // coefficient D
0.3627905f, 0.9953236f, -2.537516f, 10.283f, -4.255704f, 1.686787f
},
{ // coefficient E
-0.1039258f, -1.874457f, 5.877859f, -20.80454f, 9.903388f, -5.010957f
},
{ // coefficient F
0.4697924f, 0.4432099f, 2.014554f, -2.781026f, 3.722668f, -1.656458f
},
{ // coefficient G
0.1671653f, 0.0171581f, 0.09689141f, 0.899509f, -0.3603941f, 0.4584029f
},
{ // coefficient H
3.507497f, 2.339272f, 0.3177242f, 3.366951f, -1.303292f, -0.2751759f
},
{ // coefficient I
0.6022506f, 0.644147f, 0.9030399f, 0.3473867f, 0.9369454f, 0.6184162f
},
{ // radiance
1.650367f, 2.593201f, -4.469328f, 29.69817f, 9.410977f, 17.4485f
}
},
{ // turbidity 6
{ // coefficient A
-1.25613f, -1.620114f, -0.9686204f, -1.089377f, -1.044681f, -1.133658f
},
{ // coefficient B
-0.3104904f, -0.655267f, -0.03755647f, -0.1023062f, -0.2156857f, -0.2505101f
},
{ // coefficient C
0.163935f, -0.2877157f, 1.46998f, -1.498891f, 3.230136f, 1.71784f
},
{ // coefficient D
0.1315502f, 1.094371f, -3.103414f, 10.66455f, -0.5863862f, 0.008480428f
},
{ // coefficient E
-0.7297583f, 0.2818914f, 2.856583f, -17.20184f, 6.09664f, -5.011789f
},
{ // coefficient F
0.477848f, 0.369683f, 1.883209f, -2.759314f, 3.550019f, -1.740989f
},
{ // coefficient G
0.1259265f, 0.09428521f, -0.05746099f, 1.061258f, -0.4255773f, 0.498343f
},
{ // coefficient H
3.012108f, 1.450951f, 1.286383f, 2.910211f, -1.500033f, -0.2081829f
},
{ // coefficient I
0.6202728f, 0.5681308f, 1.001751f, 0.2624701f, 0.9687696f, 0.6088641f
},
{ // radiance
1.555202f, 2.962925f, -6.60817f, 33.29887f, 10.64559f, 18.50816f
}
},
{ // turbidity 7
{ // coefficient A
-1.335366f, -2.004511f, -0.8714157f, -1.042618f, -1.099112f, -1.106985f
},
{ // coefficient B
-0.3863319f, -0.9957121f, 0.1192051f, -0.2285793f, -0.1869868f, -0.2396881f
},
{ // coefficient C
-0.5279971f, 1.250807f, -0.8486879f, 0.3620175f, 2.044065f, 1.809807f
},
{ // coefficient D
0.3638324f, 0.01625025f, -0.3702497f, 2.983368f, 2.062964f, 8.523319f
},
{ // coefficient E
0.3230699f, -0.3410754f, 1.818277f, -9.776844f, 1.265668f, -5.011788f
},
{ // coefficient F
0.08339707f, 0.7858244f, 1.103427f, -1.971587f, 2.71013f, -1.590086f
},
{ // coefficient G
0.2483293f, -0.09506757f, 0.2454866f, 0.6691674f, -0.1099443f, 0.3248449f
},
{ // coefficient H
2.678646f, 0.02651876f, 3.841575f, -0.7901947f, 0.2179353f, -0.1003187f
},
{ // coefficient I
0.4998346f, 0.5788643f, 0.984735f, 0.32132f, 0.9024108f, 0.6550606f
},
{ // radiance
1.412478f, 3.439403f, -9.196616f, 36.85077f, 13.45341f, 20.03128f
}
},
{ // turbidity 8
{ // coefficient A
-1.421285f, -2.611217f, -0.9881543f, -0.7544759f, -1.273719f, -1.046864f
},
{ // coefficient B
-0.4767024f, -1.398846f, 0.04684782f, -0.2314808f, -0.218703f, -0.2247559f
},
{ // coefficient C
-0.3885004f, 0.4527425f, -0.8616613f, 0.812577f, 1.401798f, 1.679449f
},
{ // coefficient D
0.827459f, -0.5932142f, 0.8799807f, -0.7724135f, 5.231832f, 11.40057f
},
{ // coefficient E
-0.3644229f, 0.2224617f, 4.00313f, -9.577645f, 0.7405093f, -4.948829f
},
{ // coefficient F
0.6999513f, -0.5593581f, 1.739543f, -1.629433f, 1.775166f, -1.182664f
},
{ // coefficient G
0.0519671f, 0.3389633f, -0.08098378f, 0.6790832f, -0.07269476f, 0.3241038f
},
{ // coefficient H
2.578431f, -0.7767112f, 5.524802f, -4.193895f, 1.996087f, -0.2470012f
},
{ // coefficient I
0.624631f, 0.06536004f, 1.499673f, -0.02526624f, 1.05745f, 0.61159f
},
{ // radiance
1.25299f, 3.820805f, -11.15338f, 37.21593f, 20.14916f, 21.8232f
}
},
{ // turbidity 9
{ // coefficient A
-1.514607f, -2.308414f, -3.230837f, 0.5149378f, -1.62187f, -0.9951701f
},
{ // coefficient B
-0.598543f, -1.083797f, -1.43867f, 0.2754789f, -0.3192763f, -0.2550607f
},
{ // coefficient C
-0.187761f, -0.1179959f, -0.9868286f, 1.040965f, 0.8786242f, 1.498952f
},
{ // coefficient D
1.75693f, -1.728246f, 2.974393f, -4.501186f, 9.785565f, -0.002737197f
},
{ // coefficient E
-1.314206f, 0.7784742f, 1.949339f, -3.399057f, -2.727652f, -3.101832f
},
{ // coefficient F
0.611581f, 0.5494505f, -0.6337857f, 0.9661861f, 0.01903691f, -0.5921329f
},
{ // coefficient G
-5.97046e-06f, 0.006203168f, 0.8160271f, -0.4736173f, 0.5521261f, 0.2864422f
},
{ // coefficient H
2.412975f, 0.9326251f, 3.278606f, -4.037574f, 2.138764f, -0.4405218f
},
{ // coefficient I
0.8124304f, -0.1419518f, 1.354373f, 0.2794847f, 0.8419871f, 0.663141f
},
{ // radiance
1.091952f, 3.663027f, -10.3133f, 29.78985f, 32.96835f, 23.7545f
}
},
{ // turbidity 10
{ // coefficient A
-1.902954f, -1.271627f, -7.647175f, 2.191403f, -1.932296f, -0.9471101f
},
{ // coefficient B
-0.9056918f, -0.7193923f, -3.114129f, 0.342112f, -0.3814739f, -0.405264f
},
{ // coefficient C
-0.206957f, -0.01136606f, -1.490128f, 1.44651f, 0.6245422f, 1.300174f
},
{ // coefficient D
1.191499f, -0.1167951f, -0.5266488f, 2.170943f, 6.748294f, -0.003951536f
},
{ // coefficient E
-1.092577f, 0.003286175f, 3.06309f, -7.768187f, -0.3060171f, -1.908284f
},
{ // coefficient F
0.5849556f, -0.05262827f, 1.474262f, -1.471207f, 1.067747f, -0.5385721f
},
{ // coefficient G
-9.649602e-06f, -0.02473874f, 0.5481458f, -0.1456708f, 0.2500671f, 0.2133578f
},
{ // coefficient H
2.048407f, 1.716125f, 2.052174f, -1.753574f, -0.1252596f, -0.6250292f
},
{ // coefficient I
0.9001527f, -0.2187133f, 1.353089f, 0.2310576f, 0.8614611f, 0.6658012f
},
{ // radiance
0.9501691f, 2.664579f, -5.545167f, 12.81159f, 51.54768f, 25.74284f
}
}
}
},
{ // Green
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.14053f, -1.165117f, -1.169936f, -1.060246f, -1.089753f, -1.075481f
},
{ // coefficient B
-0.1982747f, -0.1852955f, -0.3357429f, -0.1051633f, -0.2382167f, -0.1242391f
},
{ // coefficient C
-7.51273f, 2.963684f, 1.911291f, 0.5013829f, 2.360312f, 1.425781f
},
{ // coefficient D
8.403899f, -2.262274f, -0.2391074f, 2.832309f, -5.902562f, 8.810319f
},
{ // coefficient E
-0.05699038f, -0.1571683f, -0.4791643f, -0.3707855f, -8.799894f, -2.922646f
},
{ // coefficient F
0.9015907f, 0.6339974f, 1.446113f, 1.523131f, 1.377692f, 1.48652f
},
{ // coefficient G
0.03392161f, 0.04977879f, -0.09178108f, 0.09163749f, -0.06131633f, 0.0327058f
},
{ // coefficient H
4.772522f, 7.243307f, -4.700239f, 5.604183f, -1.415472f, 3.889783f
},
{ // coefficient I
0.5111184f, 0.4220053f, 0.8096219f, 0.7208566f, 0.6124057f, 0.4999482f
},
{ // radiance
1.59033f, 1.355401f, 1.151412f, 13.59116f, 5.857714f, 8.090833f
}
},
{ // turbidity 2
{ // coefficient A
-1.149342f, -1.120531f, -1.226964f, -0.9664377f, -1.153358f, -1.05385f
},
{ // coefficient B
-0.2076337f, -0.1513311f, -0.3516378f, 0.02767589f, -0.3126223f, -0.1330743f
},
{ // coefficient C
-7.446587f, 2.735504f, 1.308298f, 0.1658235f, 2.078934f, 1.481738f
},
{ // coefficient D
8.014559f, -2.417591f, -0.05097487f, 2.407439f, -5.857733f, 10.49485f
},
{ // coefficient E
-0.04866227f, -0.1361114f, -0.4846783f, -0.1300304f, -8.659848f, -3.528854f
},
{ // coefficient F
0.8203043f, 0.4296342f, 1.654619f, 0.9170958f, 1.758505f, 0.9142363f
},
{ // coefficient G
0.06386483f, 0.09427488f, -0.113494f, 0.2742895f, -0.09616094f, 0.124488f
},
{ // coefficient H
4.894198f, 8.171403f, -3.347854f, 6.642633f, -1.230863f, 2.644615f
},
{ // coefficient I
0.5452051f, 0.4102448f, 1.131147f, 0.2550064f, 0.9663832f, 0.5001048f
},
{ // radiance
1.55254f, 1.51004f, 0.1276413f, 16.04643f, 5.912162f, 8.350009f
}
},
{ // turbidity 3
{ // coefficient A
-1.173687f, -1.213757f, -1.108794f, -1.060896f, -1.085145f, -1.077983f
},
{ // coefficient B
-0.2360362f, -0.2589561f, -0.225987f, -0.0134669f, -0.2840715f, -0.1416633f
},
{ // coefficient C
-3.741454f, 0.7132551f, 1.574179f, -0.7529656f, 2.088029f, 1.100848f
},
{ // coefficient D
4.088507f, -0.4259327f, -0.3753731f, 1.711319f, -4.935097f, 10.15875f
},
{ // coefficient E
-0.07528205f, -0.1980821f, -0.5984743f, -0.9792435f, -9.056542f, -2.943712f
},
{ // coefficient F
0.6645237f, 0.3627815f, 1.659414f, 0.2022433f, 1.976149f, 0.5255135f
},
{ // coefficient G
0.07718265f, 0.0466656f, -0.01681021f, 0.3826487f, -0.03912485f, 0.2164224f
},
{ // coefficient H
4.65122f, 5.807984f, 0.6785219f, 5.725157f, -0.8636064f, 2.941143f
},
{ // coefficient I
0.5586318f, 0.5847377f, 0.8647325f, 0.5290714f, 0.7452125f, 0.6699937f
},
{ // radiance
1.470871f, 1.880464f, -1.865398f, 20.30808f, 5.471461f, 9.109834f
}
},
{ // turbidity 4
{ // coefficient A
-1.223293f, -1.325342f, -1.113581f, -1.016063f, -1.099582f, -1.064357f
},
{ // coefficient B
-0.2867444f, -0.4280991f, -0.1192133f, -0.1038138f, -0.2228607f, -0.1520775f
},
{ // coefficient C
-1.624136f, 0.470549f, 0.4011536f, -0.2280391f, 1.332648f, 0.8207368f
},
{ // coefficient D
1.668299f, 0.06926592f, 0.7011889f, 0.7898918f, 5.135188f, -0.001323565f
},
{ // coefficient E
-0.09537589f, -0.4572587f, 0.2052842f, -11.27333f, 1.653152f, -5.009523f
},
{ // coefficient F
0.5015947f, 0.5344144f, 0.9880724f, 0.2074545f, 1.41702f, 0.3946298f
},
{ // coefficient G
0.1130741f, -0.02554192f, 0.01807533f, 0.5388182f, -0.1087532f, 0.4337902f
},
{ // coefficient H
4.244812f, 3.093939f, 4.69016f, 1.364263f, 1.809275f, 2.593198f
},
{ // coefficient I
0.5082152f, 0.6639401f, 0.857624f, 0.4660455f, 0.8080874f, 0.6719172f
},
{ // radiance
1.356563f, 2.373866f, -4.653245f, 25.70922f, 5.686009f, 10.0948f
}
},
{ // turbidity 5
{ // coefficient A
-1.278702f, -1.580069f, -0.9474023f, -1.129091f, -1.030884f, -1.075192f
},
{ // coefficient B
-0.3512866f, -0.7095475f, 0.1173703f, -0.3287694f, -0.1137581f, -0.1627166f
},
{ // coefficient C
-0.4511055f, -0.3198904f, 0.4881381f, -0.3524077f, 1.109855f, 0.351491f
},
{ // coefficient D
0.389576f, 1.715748f, -2.618684f, 3.352892f, 8.082276f, 1.168164f
},
{ // coefficient E
-0.2429672f, -1.185915f, 3.251661f, -14.16073f, 1.519214f, -4.255822f
},
{ // coefficient F
0.4270577f, 0.4523161f, 1.213931f, -0.8485617f, 2.112433f, -0.6015348f
},
{ // coefficient G
0.1135348f, -0.01026159f, -0.01736274f, 0.6560766f, -0.1592299f, 0.6265776f
},
{ // coefficient H
3.71913f, 0.7927188f, 8.000768f, -2.820937f, 3.675905f, 2.884818f
},
{ // coefficient I
0.4998867f, 0.553835f, 1.025998f, 0.3111303f, 0.8703367f, 0.6548384f
},
{ // radiance
1.244232f, 2.851519f, -7.130942f, 29.93449f, 6.38212f, 11.14578f
}
},
{ // turbidity 6
{ // coefficient A
-1.316017f, -1.731876f, -0.927368f, -1.099377f, -1.04842f, -1.063233f
},
{ // coefficient B
-0.3889652f, -0.8493806f, 0.1380954f, -0.3325392f, -0.1227773f, -0.1560639f
},
{ // coefficient C
-0.5030854f, 0.1194871f, -0.3302179f, 0.2501063f, 0.5845373f, 0.2840033f
},
{ // coefficient D
0.4488704f, 2.002781f, -3.553265f, 2.613712f, 11.05869f, 0.8751565f
},
{ // coefficient E
-0.31868f, -2.006547f, 4.633345f, -13.28142f, 0.03813151f, -3.41182f
},
{ // coefficient F
0.4570763f, 0.4872233f, 0.9696729f, -0.5579527f, 1.330409f, -0.1436564f
},
{ // coefficient G
0.08909201f, -0.02854606f, 0.08799775f, 0.4992081f, 0.01978131f, 0.584658f
},
{ // coefficient H
3.659274f, 0.2662137f, 8.291129f, -3.504402f, 3.95943f, 2.899292f
},
{ // coefficient I
0.5011746f, 0.4611629f, 1.094451f, 0.3022924f, 0.8396439f, 0.6799095f
},
{ // radiance
1.173693f, 3.120604f, -8.491886f, 31.87393f, 7.290615f, 11.80066f
}
},
{ // turbidity 7
{ // coefficient A
-1.376715f, -1.953391f, -1.012422f, -0.9706708f, -1.110611f, -1.048334f
},
{ // coefficient B
-0.4541567f, -1.035869f, 0.07136451f, -0.288095f, -0.1789409f, -0.1614087f
},
{ // coefficient C
-1.445491f, 1.690563f, -1.862534f, 1.107797f, 0.2108488f, 0.2475184f
},
{ // coefficient D
1.569898f, -0.196469f, -0.7228653f, -2.731734f, 20.7143f, 0.02146938f
},
{ // coefficient E
-0.1390627f, -0.7787096f, 0.1947997f, -8.445995f, -1.763174f, -2.983901f
},
{ // coefficient F
0.555827f, 0.5799605f, 0.2091805f, 0.4296774f, 0.09554695f, 0.2538224f
},
{ // coefficient G
0.04109877f, 0.02945626f, 0.06399233f, 0.5117648f, -0.02943103f, 0.560137f
},
{ // coefficient H
3.349451f, 0.04217906f, 7.928994f, -3.824277f, 3.422079f, 2.461925f
},
{ // coefficient I
0.5516123f, 0.2451373f, 1.290733f, 0.1761207f, 0.8815496f, 0.6777394f
},
{ // radiance
1.091845f, 3.368888f, -9.722083f, 32.68508f, 10.32424f, 12.36508f
}
},
{ // turbidity 8
{ // coefficient A
-1.393719f, -2.128663f, -1.618022f, -0.5669064f, -1.241724f, -1.02167f
},
{ // coefficient B
-0.5002724f, -1.151182f, -0.376649f, -0.1481749f, -0.2519348f, -0.1667327f
},
{ // coefficient C
-2.40894f, 2.923026f, -3.163544f, 2.071817f, -0.1908609f, 0.1789771f
},
{ // coefficient D
2.680983f, -1.931838f, 1.611608f, -8.157422f, 29.52235f, -0.002178108f
},
{ // coefficient E
-0.1362825f, -0.442617f, -0.3967476f, -5.988088f, -3.33366f, -2.641572f
},
{ // coefficient F
0.7395067f, 0.2309983f, 0.393368f, 0.2387202f, -0.01837651f, -0.05641484f
},
{ // coefficient G
-3.300343e-06f, -0.00548589f, 0.3006742f, 0.1447191f, 0.1022249f, 0.5303758f
},
{ // coefficient H
3.260889f, 0.3279529f, 6.835177f, -4.296385f, 2.92932f, 2.138196f
},
{ // coefficient I
0.8132057f, -0.2229467f, 1.613765f, 0.05011258f, 0.8867262f, 0.678035f
},
{ // radiance
0.9858985f, 3.500541f, -10.26328f, 30.92956f, 16.10881f, 13.31222f
}
},
{ // turbidity 9
{ // coefficient A
-1.669332f, -1.755806f, -3.720277f, 0.4724641f, -1.5256f, -0.9908649f
},
{ // coefficient B
-0.7588708f, -0.8735348f, -1.73335f, 0.3209828f, -0.4897606f, -0.2008182f
},
{ // coefficient C
-2.993557f, 3.258881f, -3.332272f, 2.051443f, -0.09891121f, 0.1605143f
},
{ // coefficient D
3.17876f, -2.504785f, 1.515869f, -5.105574f, 23.46818f, -0.002463113f
},
{ // coefficient E
-0.08066442f, -0.3300791f, 0.1734218f, -6.509139f, -2.278152f, -2.477349f
},
{ // coefficient F
0.6544672f, 0.1180565f, 0.8011956f, -0.4232041f, 0.1681219f, -0.1218647f
},
{ // coefficient G
-8.08988e-06f, -0.009315982f, 0.199544f, 0.2598931f, -0.04469389f, 0.4750121f
},
{ // coefficient H
2.628924f, 1.785154f, 3.817666f, -2.151756f, 1.051f, 1.460813f
},
{ // coefficient I
0.9001272f, -0.3205824f, 1.638502f, -0.00349391f, 0.9294666f, 0.6661364f
},
{ // radiance
0.8864993f, 3.172888f, -8.68755f, 23.62161f, 26.21851f, 14.74967f
}
},
{ // turbidity 10
{ // coefficient A
-2.122119f, -1.528158f, -6.524387f, 0.8886026f, -1.481997f, -1.010943f
},
{ // coefficient B
-1.125475f, -0.9370249f, -2.673507f, -0.09681828f, -0.5897282f, -0.2075134f
},
{ // coefficient C
-3.066599f, 2.567559f, -2.940472f, 1.430554f, 0.2659264f, 0.05066749f
},
{ // coefficient D
3.145078f, -1.591439f, -0.6025609f, 4.993717f, 1.267239f, 14.70708f
},
{ // coefficient E
-0.05411593f, -0.363446f, 0.7852067f, -6.071355f, -0.5741291f, -3.780501f
},
{ // coefficient F
0.5133628f, 0.1763256f, 1.073499f, -0.6053986f, 0.05983011f, 0.07253223f
},
{ // coefficient G
-7.823408e-06f, 0.001119624f, -0.03540435f, 0.5092997f, -0.2217312f, 0.4045458f
},
{ // coefficient H
2.268448f, 1.811848f, 3.517416f, -1.27301f, -0.3016452f, 1.320164f
},
{ // coefficient I
0.9001416f, -0.2637929f, 1.490466f, 0.07491329f, 0.926083f, 0.6559925f
},
{ // radiance
0.7946973f, 2.189355f, -4.207953f, 9.399091f, 40.62849f, 16.81753f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.129907f, -1.133821f, -1.176518f, -1.043563f, -1.081304f, -1.083774f
},
{ // coefficient B
-0.1884011f, -0.1510781f, -0.3549902f, -0.07520975f, -0.2222253f, -0.1375469f
},
{ // coefficient C
-8.04767f, 3.362822f, 1.729044f, 0.8750644f, 2.517638f, 1.685818f
},
{ // coefficient D
9.035776f, -2.453381f, -0.2160966f, 2.510518f, -4.45382f, 5.63112f
},
{ // coefficient E
-0.05539419f, -0.1463925f, -0.5075865f, 0.007584882f, -8.663691f, -3.100752f
},
{ // coefficient F
0.8823349f, 0.4728708f, 1.675584f, 0.936125f, 0.8662558f, 0.4045941f
},
{ // coefficient G
0.03197135f, 0.0595814f, -0.08906902f, 0.07889083f, -0.04802657f, 0.02346895f
},
{ // coefficient H
4.839388f, 7.6363f, -5.386842f, 6.066644f, -0.8965449f, 3.390321f
},
{ // coefficient I
0.5042822f, 0.4805162f, 0.5452218f, 0.5813108f, 0.4886656f, 0.5008309f
},
{ // radiance
1.711696f, 1.657311f, 0.9328021f, 13.1788f, 15.06751f, 18.63556f
}
},
{ // turbidity 2
{ // coefficient A
-1.143158f, -1.146773f, -1.151324f, -1.034258f, -1.096568f, -1.083304f
},
{ // coefficient B
-0.2058334f, -0.1727385f, -0.3099303f, -0.007778759f, -0.2673169f, -0.1528265f
},
{ // coefficient C
-9.660198f, 4.626048f, 0.4125596f, 0.7050094f, 2.575654f, 1.697727f
},
{ // coefficient D
10.62394f, -4.684602f, 2.340752f, -0.8036369f, -0.8057121f, 2.503702f
},
{ // coefficient E
-0.04434119f, -0.08307137f, -0.4214444f, 0.313857f, -8.884928f, -2.885296f
},
{ // coefficient F
0.8607615f, 0.1619616f, 1.987375f, 0.2469452f, 1.41617f, -0.12985f
},
{ // coefficient G
0.03177325f, 0.1484866f, -0.191341f, 0.355997f, -0.2091315f, 0.154887f
},
{ // coefficient H
4.416481f, 7.572868f, -3.845978f, 7.485917f, -1.543494f, 2.479652f
},
{ // coefficient I
0.5918162f, 0.2681126f, 1.337311f, 0.04790329f, 1.065445f, 0.5066496f
},
{ // radiance
1.666968f, 1.849993f, -0.2088601f, 15.86653f, 14.8688f, 19.40719f
}
},
{ // turbidity 3
{ // coefficient A
-1.165736f, -1.212422f, -1.070436f, -1.104879f, -1.048969f, -1.106145f
},
{ // coefficient B
-0.2329945f, -0.254591f, -0.2163341f, -0.03013622f, -0.2655317f, -0.1601642f
},
{ // coefficient C
-5.967964f, 2.418626f, 0.7098718f, 0.02976276f, 2.689426f, 1.544544f
},
{ // coefficient D
6.705959f, -2.266104f, 0.7843075f, 1.069707f, -3.941038f, 7.388492f
},
{ // coefficient E
-0.05931355f, -0.1102014f, -0.432393f, 0.141f, -9.506461f, -2.9246f
},
{ // coefficient F
0.7485638f, 0.01363887f, 2.109823f, -0.488002f, 1.837119f, -0.4328453f
},
{ // coefficient G
0.03913878f, 0.1055411f, -0.095897f, 0.4452288f, -0.1892124f, 0.1763161f
},
{ // coefficient H
4.221591f, 5.648062f, -0.1985193f, 6.41859f, -1.562146f, 2.523111f
},
{ // coefficient I
0.6183926f, 0.4557412f, 1.060428f, 0.3195986f, 0.9043414f, 0.5851902f
},
{ // radiance
1.584846f, 2.170022f, -2.019597f, 19.70826f, 14.90684f, 20.45055f
}
},
{ // turbidity 4
{ // coefficient A
-1.203666f, -1.340448f, -1.014554f, -1.137388f, -1.021816f, -1.126581f
},
{ // coefficient B
-0.2776587f, -0.423059f, -0.116879f, -0.0985403f, -0.2344957f, -0.1874347f
},
{ // coefficient C
-2.084286f, 0.3462849f, 0.9477794f, -0.2984893f, 2.463671f, 1.454154f
},
{ // coefficient D
2.45084f, 0.4707607f, -1.061218f, 3.64782f, -7.240685f, 10.34398f
},
{ // coefficient E
-0.08746613f, -0.2512626f, -0.419673f, -0.6585571f, -8.862697f, -3.237393f
},
{ // coefficient F
0.5258507f, 0.1530746f, 2.058832f, -1.47918f, 2.514058f, -0.8654927f
},
{ // coefficient G
0.07983316f, 0.02724218f, -0.05989624f, 0.6102932f, -0.2122768f, 0.2457248f
},
{ // coefficient H
3.860055f, 3.035216f, 3.058168f, 3.265914f, -0.03313968f, 1.845769f
},
{ // coefficient I
0.5486167f, 0.5876133f, 0.9763861f, 0.3480333f, 0.9028136f, 0.6002482f
},
{ // radiance
1.469412f, 2.524017f, -4.197267f, 23.65249f, 16.64588f, 21.34477f
}
},
{ // turbidity 5
{ // coefficient A
-1.263727f, -1.53001f, -0.9685659f, -1.113925f, -1.046864f, -1.10448f
},
{ // coefficient B
-0.3439354f, -0.6879698f, 0.09480403f, -0.3112382f, -0.1215754f, -0.1940913f
},
{ // coefficient C
-0.1786388f, 0.2380415f, 0.06076844f, 0.3954133f, 1.778308f, 1.343668f
},
{ // coefficient D
0.3980166f, 1.608216f, -3.217561f, 5.105907f, 4.661489f, -0.001759206f
},
{ // coefficient E
-0.3349517f, -1.682679f, 4.568074f, -14.56866f, 0.2565583f, -5.009204f
},
{ // coefficient F
0.3825166f, 0.354636f, 1.069299f, -0.4917378f, 1.35368f, -0.4186951f
},
{ // coefficient G
0.1029225f, -0.00391522f, 0.02083638f, 0.5289909f, -0.1175767f, 0.312571f
},
{ // coefficient H
3.331096f, 0.4517655f, 7.301088f, -2.678374f, 3.415972f, 1.628183f
},
{ // coefficient I
0.4998955f, 0.5128605f, 1.072165f, 0.3014709f, 0.8457746f, 0.6720408f
},
{ // radiance
1.369714f, 2.843548f, -6.059031f, 26.34993f, 18.81361f, 22.32186f
}
},
{ // turbidity 6
{ // coefficient A
-1.286902f, -1.712597f, -0.8807283f, -1.147253f, -1.03823f, -1.101026f
},
{ // coefficient B
-0.3781238f, -0.8549112f, 0.1646097f, -0.3538199f, -0.1213475f, -0.1874907f
},
{ // coefficient C
-0.08977253f, 0.4809286f, -0.4437898f, 0.6101836f, 1.547505f, 1.272667f
},
{ // coefficient D
0.3545393f, 1.515398f, -3.188247f, 4.43778f, 5.893176f, 3.596524f
},
{ // coefficient E
-0.4866515f, -2.212211f, 5.984417f, -15.59813f, 1.368738f, -5.007243f
},
{ // coefficient F
0.3843664f, 0.2539029f, 1.334779f, -1.103222f, 1.663127f, -0.6352483f
},
{ // coefficient G
0.08281675f, 0.02335997f, -0.04026975f, 0.6242039f, -0.137713f, 0.3048985f
},
{ // coefficient H
3.122231f, -0.06089466f, 7.546431f, -3.091472f, 3.185279f, 1.931613f
},
{ // coefficient I
0.5046991f, 0.4268444f, 1.175751f, 0.217429f, 0.8736453f, 0.6788844f
},
{ // radiance
1.310477f, 2.984444f, -6.831686f, 26.8234f, 21.23267f, 22.59755f
}
},
{ // turbidity 7
{ // coefficient A
-1.342753f, -1.937684f, -0.9302391f, -1.053631f, -1.083629f, -1.09395f
},
{ // coefficient B
-0.4384971f, -1.066019f, 0.1486187f, -0.372705f, -0.1261405f, -0.2067455f
},
{ // coefficient C
-1.213491f, 1.913336f, -1.603835f, 1.114117f, 1.229344f, 1.258035f
},
{ // coefficient D
1.621399f, -0.7347719f, 0.1783713f, -0.9603286f, 11.27825f, 7.548645f
},
{ // coefficient E
-0.1551441f, -0.5916167f, 1.100461f, -10.62469f, 0.131901f, -4.598387f
},
{ // coefficient F
0.5614218f, 0.158759f, 1.174181f, -1.16214f, 1.624729f, -0.8944932f
},
{ // coefficient G
0.02591739f, 0.1092568f, -0.1602361f, 0.7952797f, -0.2825898f, 0.3292634f
},
{ // coefficient H
2.958967f, -0.6275002f, 7.868331f, -4.478765f, 3.661082f, 1.311304f
},
{ // coefficient I
0.5782132f, 0.1599071f, 1.468971f, -0.04440862f, 1.036911f, 0.6291871f
},
{ // radiance
1.222552f, 3.176523f, -7.731496f, 26.7176f, 24.84358f, 23.36863f
}
},
{ // turbidity 8
{ // coefficient A
-1.385867f, -2.107367f, -1.475244f, -0.6644372f, -1.211687f, -1.063757f
},
{ // coefficient B
-0.5068139f, -1.175639f, -0.2833055f, -0.2549735f, -0.1653494f, -0.2037563f
},
{ // coefficient C
-1.48649f, 2.313241f, -2.085824f, 1.600273f, 0.83934f, 1.117207f
},
{ // coefficient D
1.969049f, -1.001653f, 1.192563f, -8.589034f, 27.92988f, -0.001252806f
},
{ // coefficient E
-0.1698025f, -0.4843139f, -0.76452f, -6.144718f, -3.395461f, -3.33233f
},
{ // coefficient F
0.6629167f, 0.1124485f, 0.8380081f, -0.7599731f, 0.9933752f, -0.6971409f
},
{ // coefficient G
-5.289365e-06f, 3.901494e-05f, 0.220358f, 0.289837f, -0.03976877f, 0.3388719f
},
{ // coefficient H
2.760315f, -0.3502469f, 7.157885f, -5.770923f, 3.776659f, 1.311398f
},
{ // coefficient I
0.8644368f, -0.320478f, 1.753702f, -0.09656242f, 0.9546526f, 0.6635171f
},
{ // radiance
1.115781f, 3.130635f, -7.581744f, 23.36531f, 31.71048f, 24.13859f
}
},
{ // turbidity 9
{ // coefficient A
-1.678889f, -1.692144f, -3.676795f, 0.5424542f, -1.553849f, -1.02065f
},
{ // coefficient B
-0.7992295f, -0.880425f, -1.719207f, 0.3263528f, -0.457686f, -0.2215797f
},
{ // coefficient C
-2.421687f, 3.060895f, -2.534697f, 1.551055f, 0.9324676f, 1.009774f
},
{ // coefficient D
2.871029f, -2.000009f, 1.005285f, -3.841058f, 19.50982f, -0.002056855f
},
{ // coefficient E
-0.07662842f, -0.3183563f, 0.1550407f, -6.598996f, -2.344516f, -2.740338f
},
{ // coefficient F
0.6046208f, 0.08385862f, 1.07291f, -1.201779f, 1.12102f, -0.8122355f
},
{ // coefficient G
-7.598099e-06f, -0.006326713f, 0.1318094f, 0.3530669f, -0.1221537f, 0.3328967f
},
{ // coefficient H
2.002314f, 1.206639f, 3.717018f, -2.542945f, 0.7285496f, 0.8982766f
},
{ // coefficient I
0.9001307f, -0.3369967f, 1.689191f, -0.06482523f, 0.9582816f, 0.6594676f
},
{ // radiance
1.013181f, 2.699342f, -5.602709f, 15.00158f, 42.17613f, 25.15957f
}
},
{ // turbidity 10
{ // coefficient A
-2.24736f, -1.248499f, -7.004119f, 1.434882f, -1.559053f, -1.044436f
},
{ // coefficient B
-1.221267f, -0.8442718f, -2.927978f, 0.09114639f, -0.5502302f, -0.2360719f
},
{ // coefficient C
-3.072346f, 3.062611f, -2.649852f, 1.170089f, 1.200396f, 0.7054471f
},
{ // coefficient D
3.385139f, -2.020314f, 0.7971894f, 0.03512808f, 13.47741f, -0.002904518f
},
{ // coefficient E
-0.04387559f, -0.2815341f, 0.5466893f, -5.861915f, -2.344397f, -2.092829f
},
{ // coefficient F
0.5084887f, 0.05254745f, 1.442667f, -1.411843f, 0.8868907f, -0.5119668f
},
{ // coefficient G
-7.418833e-06f, 0.003345008f, -0.06063912f, 0.5400486f, -0.3292661f, 0.4174861f
},
{ // coefficient H
1.750107f, 1.433225f, 2.806194f, -0.7746522f, -1.362105f, 0.9687435f
},
{ // coefficient I
0.9001401f, -0.2835911f, 1.547429f, 0.02386984f, 0.9217826f, 0.6588427f
},
{ // radiance
0.8976323f, 1.726948f, -1.29612f, 1.183675f, 55.03215f, 26.43066f
}
}
}
},
{ // Blue
{ // albedo 0
{ // turbidity 1
{ // coefficient A
-1.372629f, -1.523025f, -1.035567f, -1.271086f, -0.9712598f, -1.087457f
},
{ // coefficient B
-0.4905585f, -0.6497084f, -0.0747874f, -0.558819f, -0.07033926f, -0.1888896f
},
{ // coefficient C
-41.00789f, 6.249857f, 0.922103f, 0.6908023f, 0.9167274f, 0.8156686f
},
{ // coefficient D
41.22169f, -5.662543f, -2.140047f, 2.096832f, -0.9502097f, 0.3101712f
},
{ // coefficient E
-0.00738936f, -0.01908402f, -0.02374146f, -0.2453967f, 0.3004684f, -2.155419f
},
{ // coefficient F
0.4839359f, 0.551281f, 0.3795517f, 1.410648f, 0.4547054f, 1.422205f
},
{ // coefficient G
0.006474757f, -2.181049e-05f, -0.01769134f, 0.04475036f, -0.05929017f, 0.09692261f
},
{ // coefficient H
3.471755f, 2.507663f, 7.479831f, -4.719115f, 5.266196f, 3.122404f
},
{ // coefficient I
0.5092936f, 0.4339598f, 0.7729303f, 0.5741186f, 0.7204135f, 0.499943f
},
{ // radiance
0.9926518f, 1.999494f, -4.136109f, 18.5627f, 13.51028f, 13.90238f
}
},
{ // turbidity 2
{ // coefficient A
-1.42528f, -1.688557f, -0.8664152f, -1.319763f, -0.9706008f, -1.08533f
},
{ // coefficient B
-0.5413508f, -0.8070865f, 0.07869125f, -0.5985323f, -0.05914059f, -0.1956105f
},
{ // coefficient C
-34.54883f, 7.018459f, -0.5236535f, 1.253918f, 0.569315f, 0.8019605f
},
{ // coefficient D
34.81142f, -6.244574f, -1.21896f, 1.914706f, -1.175362f, 0.5338101f
},
{ // coefficient E
-0.008686975f, -0.02149341f, -0.02059093f, -0.3216739f, 0.5221644f, -3.423464f
},
{ // coefficient F
0.4914268f, 0.3993971f, 0.6684898f, 0.9011213f, 0.7518213f, 1.110444f
},
{ // coefficient G
-2.479243e-06f, 0.01252502f, -0.05584112f, 0.1324845f, -0.08247655f, 0.1507923f
},
{ // coefficient H
3.239879f, 1.630662f, 8.602299f, -5.252749f, 5.875635f, 2.864942f
},
{ // coefficient I
0.6094201f, 0.109786f, 1.410496f, 0.06231252f, 0.9850863f, 0.4999481f
},
{ // radiance
0.9634366f, 2.119694f, -4.614523f, 19.19701f, 13.76644f, 14.18731f
}
},
{ // turbidity 3
{ // coefficient A
-1.431967f, -1.801361f, -0.7905357f, -1.308575f, -0.9829872f, -1.08713f
},
{ // coefficient B
-0.5478935f, -0.9315889f, 0.1451332f, -0.5539232f, -0.08183048f, -0.2038013f
},
{ // coefficient C
-32.86288f, 5.391756f, -0.1605661f, 0.9184133f, 0.4464556f, 0.7260801f
},
{ // coefficient D
33.05288f, -4.588592f, -1.592174f, 2.011479f, -1.442716f, 0.9164376f
},
{ // coefficient E
-0.008380797f, -0.02040076f, 0.0004561348f, -0.3842472f, 1.029641f, -5.006183f
},
{ // coefficient F
0.477205f, 0.4144684f, 0.3380323f, 1.432274f, -0.06991617f, 1.511271f
},
{ // coefficient G
-3.044274e-06f, 0.01814534f, -0.07770275f, 0.1637153f, 0.008702356f, 0.1257134f
},
{ // coefficient H
3.289973f, 1.051795f, 8.775384f, -4.408856f, 5.706417f, 2.715439f
},
{ // coefficient I
0.5976303f, 0.1145651f, 1.489512f, 0.05272957f, 0.9116452f, 0.6201652f
},
{ // radiance
0.9446537f, 2.17161f, -4.915556f, 19.1824f, 15.37135f, 14.0053f
}
},
{ // turbidity 4
{ // coefficient A
-1.448662f, -2.061772f, -0.6413755f, -1.304871f, -1.001104f, -1.111552f
},
{ // coefficient B
-0.5799075f, -1.14519f, 0.1780425f, -0.3975581f, -0.1994801f, -0.2487204f
},
{ // coefficient C
-28.33268f, 7.918478f, -2.412919f, 1.219002f, 0.3676742f, 0.7410842f
},
{ // coefficient D
28.58023f, -7.212525f, 1.064484f, 0.7285178f, -1.409737f, 1.703749f
},
{ // coefficient E
-0.009134061f, -0.0202076f, -0.01949986f, -0.2710105f, 0.2901555f, -5.007855f
},
{ // coefficient F
0.4404783f, 0.2962715f, 0.6769741f, 0.7779727f, 0.250694f, 1.057763f
},
{ // coefficient G
-2.709026e-06f, 0.0468967f, -0.175276f, 0.3247139f, 0.002468899f, 0.1354511f
},
{ // coefficient H
3.029357f, 0.8517209f, 7.262714f, -0.8818168f, 3.398923f, 2.088715f
},
{ // coefficient I
0.5540071f, 0.2334587f, 1.325869f, 0.1839517f, 0.8584645f, 0.6600013f
},
{ // radiance
0.9073074f, 2.330536f, -5.577596f, 19.61615f, 16.88365f, 14.46955f
}
},
{ // turbidity 5
{ // coefficient A
-1.547227f, -2.04389f, -0.904072f, -1.000013f, -1.171332f, -1.068667f
},
{ // coefficient B
-0.6679466f, -1.149081f, -0.08274472f, -0.1010774f, -0.3768188f, -0.232633f
},
{ // coefficient C
-18.61465f, 2.304118f, -0.2555676f, 0.3699166f, 0.3701377f, 0.6725059f
},
{ // coefficient D
18.84045f, -1.715757f, -0.6326215f, 0.8774526f, -1.470757f, 2.243733f
},
{ // coefficient E
-0.0124221f, -0.02433628f, -0.0277088f, -0.3042007f, 0.5525942f, -4.61437f
},
{ // coefficient F
0.4157339f, 0.2816836f, 0.6676024f, 0.6951053f, 0.02991456f, 1.033677f
},
{ // coefficient G
-2.432805e-06f, 0.07185458f, -0.2513532f, 0.4361813f, 0.01581823f, 0.1376291f
},
{ // coefficient H
2.812423f, 1.06486f, 5.903839f, 0.6793421f, 2.365233f, 2.013334f
},
{ // coefficient I
0.5446957f, 0.2706789f, 1.241452f, 0.2573892f, 0.8214514f, 0.6865304f
},
{ // radiance
0.8739124f, 2.388682f, -5.842995f, 19.23265f, 18.87735f, 14.85698f
}
},
{ // turbidity 6
{ // coefficient A
-1.592991f, -2.271349f, -0.636862f, -1.177925f, -1.0785f, -1.108028f
},
{ // coefficient B
-0.7246948f, -1.280884f, -0.007436052f, -0.09628114f, -0.3801779f, -0.2596892f
},
{ // coefficient C
-25.98204f, 6.308739f, -2.230026f, 0.3051152f, 0.4788906f, 0.5162202f
},
{ // coefficient D
26.2196f, -5.75835f, 1.640997f, -0.03749544f, -0.4795969f, 1.557081f
},
{ // coefficient E
-0.008365176f, -0.01977049f, -0.01548497f, -0.2713209f, 0.5977621f, -4.265039f
},
{ // coefficient F
0.4207571f, 0.3671835f, 0.3145331f, 1.164226f, -0.4488535f, 1.182535f
},
{ // coefficient G
-2.742772e-06f, 0.06698038f, -0.2492644f, 0.4559969f, 0.03386874f, 0.1563762f
},
{ // coefficient H
2.623735f, 1.150597f, 5.083843f, 2.175429f, 1.538143f, 2.095084f
},
{ // coefficient I
0.587319f, 0.1759218f, 1.260215f, 0.2874284f, 0.8062054f, 0.6883383f
},
{ // radiance
0.8563688f, 2.391534f, -5.769133f, 18.28709f, 20.97209f, 14.69587f
}
},
{ // turbidity 7
{ // coefficient A
-1.668427f, -2.156175f, -1.117051f, -0.744484f, -1.273036f, -1.084192f
},
{ // coefficient B
-0.7908511f, -1.220004f, -0.4101627f, 0.2312078f, -0.5275562f, -0.2936753f
},
{ // coefficient C
-27.7969f, 3.585732f, -0.846373f, -0.5393724f, 0.4902512f, 0.4719432f
},
{ // coefficient D
27.99746f, -3.235988f, 0.7671472f, 0.1574213f, -0.04498436f, 1.384436f
},
{ // coefficient E
-0.007186935f, -0.01086239f, -0.02226609f, -0.1763914f, 0.4339366f, -3.257789f
},
{ // coefficient F
0.3757766f, 0.1846143f, 0.8574943f, 0.2751692f, 0.2386682f, 0.6119543f
},
{ // coefficient G
-3.326858e-06f, 0.1046017f, -0.3434124f, 0.55642f, 0.02380879f, 0.1681884f
},
{ // coefficient H
2.563421f, 1.234427f, 4.475715f, 2.217672f, 1.413444f, 1.650441f
},
{ // coefficient I
0.5439687f, 0.2842191f, 1.154824f, 0.3483932f, 0.7855923f, 0.6936631f
},
{ // radiance
0.8270533f, 2.34279f, -5.558071f, 16.84993f, 23.56498f, 15.05975f
}
},
{ // turbidity 8
{ // coefficient A
-1.84849f, -2.300008f, -1.483705f, -0.5348005f, -1.320401f, -1.10104f
},
{ // coefficient B
-0.951267f, -1.252335f, -0.8547575f, 0.3982733f, -0.6043247f, -0.3420019f
},
{ // coefficient C
-30.05251f, -1.218876f, -0.7797146f, -0.4071573f, 0.3019196f, 0.3775661f
},
{ // coefficient D
30.24315f, 1.49373f, 0.6447971f, 0.3265569f, -0.07732911f, 1.769338f
},
{ // coefficient E
-0.005635304f, -0.0061071f, -0.02678052f, -0.08658789f, 0.4768381f, -2.990515f
},
{ // coefficient F
0.344778f, 0.0797486f, 1.091263f, -0.2370892f, 0.6745764f, 0.1649529f
},
{ // coefficient G
-2.782999e-06f, 0.1023449f, -0.3344889f, 0.5369097f, 0.03694098f, 0.1970125f
},
{ // coefficient H
2.309422f, 1.505934f, 3.830416f, 1.478279f, 1.158234f, 1.453355f
},
{ // coefficient I
0.5643559f, 0.2360948f, 1.189425f, 0.3143303f, 0.8169056f, 0.6759757f
},
{ // radiance
0.7908339f, 2.190341f, -4.852571f, 13.74862f, 28.06846f, 15.48444f
}
},
{ // turbidity 9
{ // coefficient A
-2.251946f, -1.867834f, -3.398629f, 0.5708381f, -1.705909f, -1.012865f
},
{ // coefficient B
-1.229349f, -0.9531252f, -2.180862f, 0.940603f, -0.8517224f, -0.3495551f
},
{ // coefficient C
-32.71808f, -12.29365f, 2.335213f, -0.6890113f, 0.113116f, 0.3369038f
},
{ // coefficient D
32.83114f, 12.69149f, -3.382823f, 2.746233f, -2.141434f, 3.724205f
},
{ // coefficient E
-0.004252027f, -0.006844772f, -0.008613985f, -0.05772068f, 0.4274043f, -3.089586f
},
{ // coefficient F
0.3372289f, 0.1185107f, 0.8431602f, 0.1096005f, 0.33976f, 0.1266964f
},
{ // coefficient G
-3.001937e-06f, 0.07539587f, -0.2393567f, 0.3491978f, 0.178649f, 0.146179f
},
{ // coefficient H
2.154046f, 1.846381f, 3.11246f, 0.7281453f, 0.9026101f, 1.170199f
},
{ // coefficient I
0.5842674f, 0.1899412f, 1.218556f, 0.3212049f, 0.78828f, 0.6931052f
},
{ // radiance
0.7403619f, 1.783998f, -2.983854f, 7.622563f, 35.0761f, 16.15805f
}
},
{ // turbidity 10
{ // coefficient A
-2.890318f, -1.956022f, -5.612198f, 0.8353756f, -1.524329f, -1.070371f
},
{ // coefficient B
-1.665573f, -1.0629f, -3.101371f, 0.4855943f, -0.7388547f, -0.4207858f
},
{ // coefficient C
-34.93756f, -19.19714f, 4.098034f, -1.243589f, 0.3259025f, 0.1739862f
},
{ // coefficient D
35.00369f, 19.75164f, -6.144001f, 5.147385f, -4.050634f, 5.29341f
},
{ // coefficient E
-0.002984251f, -0.008865396f, 0.009944958f, -0.07013963f, 0.4058549f, -3.136757f
},
{ // coefficient F
0.2622419f, 0.216554f, 0.2905472f, 0.938041f, -0.2591384f, 0.2323856f
},
{ // coefficient G
-4.25936e-06f, 0.05475637f, -0.170711f, 0.2335714f, 0.1898299f, 0.1673706f
},
{ // coefficient H
1.947681f, 1.761134f, 3.199107f, 0.1727744f, 0.3556071f, 1.007227f
},
{ // coefficient I
0.6905752f, 0.003164249f, 1.33766f, 0.2802696f, 0.7884126f, 0.6844287f
},
{ // radiance
0.6840111f, 1.154457f, -0.239383f, -0.7896893f, 42.82765f, 17.79469f
}
}
},
{ // albedo 1
{ // turbidity 1
{ // coefficient A
-1.34172f, -1.529104f, -1.014776f, -1.172413f, -0.9898161f, -1.085111f
},
{ // coefficient B
-0.4834889f, -0.6498631f, -0.1454495f, -0.402632f, -0.05772814f, -0.1882675f
},
{ // coefficient C
-46.33447f, 15.34103f, -4.071085f, 2.960428f, 0.4470024f, 1.223748f
},
{ // coefficient D
46.82148f, -14.50675f, 2.954982f, 0.202071f, -0.5786656f, 0.3565495f
},
{ // coefficient E
-0.006137296f, -0.01531439f, -0.02630348f, -0.2004947f, 0.1158168f, -3.688357f
},
{ // coefficient F
0.4599216f, 0.3280082f, 0.5681531f, 0.9375572f, 0.346804f, 0.5653723f
},
{ // coefficient G
0.007047323f, 0.01682926f, -0.03016505f, 0.05998168f, -0.0504336f, 0.06727646f
},
{ // coefficient H
2.895798f, 1.901587f, 6.773854f, -4.945934f, 6.867947f, 2.69013f
},
{ // coefficient I
0.4999398f, 0.5013227f, 0.5003504f, 0.4502898f, 0.8012363f, 0.49994f
},
{ // radiance
1.1683f, 1.860993f, -2.129074f, 12.51952f, 30.32499f, 29.38716f
}
},
{ // turbidity 2
{ // coefficient A
-1.389119f, -1.584641f, -0.9826068f, -1.150423f, -1.016933f, -1.073151f
},
{ // coefficient B
-0.529025f, -0.7200619f, -0.08887254f, -0.4073793f, -0.06311501f, -0.1845444f
},
{ // coefficient C
-40.55774f, 12.48067f, -2.960031f, 2.412991f, 0.5218937f, 1.155394f
},
{ // coefficient D
41.05972f, -11.56028f, 1.808816f, 0.487084f, -0.571643f, 0.3004486f
},
{ // coefficient E
-0.007062577f, -0.01659568f, -0.02478159f, -0.2337902f, 0.1250993f, -3.431711f
},
{ // coefficient F
0.456006f, 0.3050029f, 0.6035733f, 0.8295114f, 0.3601524f, 0.4657031f
},
{ // coefficient G
-1.736334e-06f, 0.01099895f, -0.04868441f, 0.1129914f, -0.05497586f, 0.09401223f
},
{ // coefficient H
2.775512f, 1.438927f, 7.347705f, -5.150045f, 7.060139f, 2.68862f
},
{ // coefficient I
0.6671455f, -0.02138015f, 1.584739f, -0.09016643f, 1.018333f, 0.4999544f
},
{ // radiance
1.150338f, 1.918813f, -2.413527f, 12.74862f, 30.87134f, 29.51432f
}
},
{ // turbidity 3
{ // coefficient A
-1.391257f, -1.780062f, -0.7388169f, -1.322387f, -0.9378288f, -1.100832f
},
{ // coefficient B
-0.5365815f, -0.922888f, 0.127567f, -0.5320143f, -0.01599895f, -0.2172313f
},
{ // coefficient C
-42.55881f, 13.76172f, -3.999528f, 2.659359f, 0.3607555f, 1.211561f
},
{ // coefficient D
42.99132f, -12.60946f, 2.223993f, 1.086712f, -1.980561f, 2.002721f
},
{ // coefficient E
-0.005838466f, -0.01507526f, -0.01856853f, -0.2129712f, 0.3791456f, -5.010011f
},
{ // coefficient F
0.4229134f, 0.3117435f, 0.543931f, 0.8704649f, 0.1212268f, 0.5717583f
},
{ // coefficient G
-2.760038e-06f, 0.02205045f, -0.08834054f, 0.1800315f, -0.02845992f, 0.06777702f
},
{ // coefficient H
2.775531f, 0.6093731f, 8.037139f, -4.967241f, 6.825542f, 2.160006f
},
{ // coefficient I
0.6234597f, 0.03463446f, 1.645951f, -0.138372f, 1.059139f, 0.5676392f
},
{ // radiance
1.114719f, 1.964689f, -2.625423f, 12.47837f, 32.37949f, 29.43596f
}
},
{ // turbidity 4
{ // coefficient A
-1.409373f, -1.954312f, -0.6772215f, -1.2916f, -0.9750857f, -1.088007f
},
{ // coefficient B
-0.5708751f, -1.11651f, 0.2001396f, -0.4977437f, -0.0877056f, -0.1959301f
},
{ // coefficient C
-30.34974f, 5.399148f, -0.3670523f, 0.9641914f, 0.9054256f, 0.9745799f
},
{ // coefficient D
30.79809f, -4.299553f, -1.014628f, 1.56242f, -1.429236f, 1.260761f
},
{ // coefficient E
-0.007280715f, -0.01724739f, -0.003497152f, -0.3227782f, 0.8974777f, -5.008864f
},
{ // coefficient F
0.3723304f, 0.3742824f, 0.4099858f, 0.9055427f, -0.1217961f, 0.7271248f
},
{ // coefficient G
-2.436279e-06f, 0.04187077f, -0.1584633f, 0.3046444f, -0.05194608f, 0.1096661f
},
{ // coefficient H
2.577348f, 0.1044883f, 7.7504f, -3.385619f, 4.909409f, 2.717295f
},
{ // coefficient I
0.5913377f, 0.1232727f, 1.514559f, 0.009546291f, 0.9589153f, 0.6340731f
},
{ // radiance
1.077948f, 2.006292f, -2.846934f, 11.90195f, 34.59293f, 29.37492f
}
},
{ // turbidity 5
{ // coefficient A
-1.45605f, -2.176449f, -0.5789641f, -1.307463f, -0.9698678f, -1.104349f
},
{ // coefficient B
-0.6223072f, -1.302416f, 0.2215407f, -0.4515174f, -0.1307094f, -0.2380323f
},
{ // coefficient C
-22.28088f, 2.222836f, 0.2142291f, 0.6447827f, 0.9389347f, 1.047043f
},
{ // coefficient D
22.69604f, -1.22273f, -1.201725f, 1.223841f, -1.522852f, 1.865421f
},
{ // coefficient E
-0.009340812f, -0.01728051f, -0.01185728f, -0.2902391f, 0.7768797f, -5.011664f
},
{ // coefficient F
0.4118308f, 0.1323513f, 0.8122982f, 0.4986588f, -0.1368595f, 0.7014954f
},
{ // coefficient G
-2.418083e-06f, 0.07027731f, -0.238042f, 0.4073652f, -0.03857426f, 0.09622701f
},
{ // coefficient H
2.442117f, 0.04835745f, 6.706841f, -1.706696f, 3.676935f, 1.89136f
},
{ // coefficient I
0.5589638f, 0.2093351f, 1.404146f, 0.1060885f, 0.8980966f, 0.6687354f
},
{ // radiance
1.035143f, 1.986681f, -2.752584f, 10.60972f, 37.22185f, 29.18594f
}
},
{ // turbidity 6
{ // coefficient A
-1.502249f, -2.217125f, -0.6215757f, -1.216317f, -1.030437f, -1.078984f
},
{ // coefficient B
-0.6724523f, -1.364924f, 0.1687995f, -0.3489429f, -0.2034064f, -0.2070549f
},
{ // coefficient C
-28.88092f, 4.048243f, -0.5949131f, 0.7566226f, 0.8335995f, 0.9683145f
},
{ // coefficient D
29.3036f, -3.111333f, -0.1551293f, 0.5409809f, -1.050947f, 1.497022f
},
{ // coefficient E
-0.006685766f, -0.01317747f, 0.0003356129f, -0.2830843f, 0.8689093f, -5.007653f
},
{ // coefficient F
0.3685464f, 0.1921948f, 0.6897657f, 0.6191825f, -0.367231f, 0.7702541f
},
{ // coefficient G
-2.469442e-06f, 0.08627702f, -0.2855053f, 0.4755163f, -0.04056183f, 0.1285822f
},
{ // coefficient H
2.310797f, 0.001981769f, 6.271042f, -0.9131387f, 3.111269f, 2.225188f
},
{ // coefficient I
0.5566754f, 0.2213689f, 1.363084f, 0.1383909f, 0.8856842f, 0.6587911f
},
{ // radiance
1.015992f, 1.992054f, -2.812626f, 10.01416f, 38.473f, 29.24624f
}
},
{ // turbidity 7
{ // coefficient A
-1.559291f, -2.356929f, -0.6589752f, -1.161046f, -1.034427f, -1.109954f
},
{ // coefficient B
-0.7374039f, -1.444755f, -0.0292691f, -0.1472506f, -0.3062504f, -0.2740277f
},
{ // coefficient C
-35.96311f, 6.244526f, -1.831779f, 0.6494138f, 0.8975634f, 1.063811f
},
{ // coefficient D
36.3447f, -5.540162f, 1.869962f, -0.8327174f, 0.3203531f, 0.7077398f
},
{ // coefficient E
-0.004667132f, -0.00879451f, -0.002030095f, -0.2320724f, 0.8565142f, -4.695734f
},
{ // coefficient F
0.3277964f, 0.17921f, 0.7552089f, 0.3391212f, -0.1250162f, 0.5621696f
},
{ // coefficient G
-2.487945e-06f, 0.09578517f, -0.3168157f, 0.5269637f, -0.04094017f, 0.1248956f
},
{ // coefficient H
2.215652f, 0.3737676f, 4.632196f, 0.9376341f, 1.861304f, 1.297723f
},
{ // coefficient I
0.5764681f, 0.1922194f, 1.294054f, 0.2458573f, 0.8223468f, 0.678972f
},
{ // radiance
0.9756887f, 1.939897f, -2.533281f, 8.319176f, 40.83907f, 29.25586f
}
},
{ // turbidity 8
{ // coefficient A
-1.788293f, -2.268206f, -1.376966f, -0.601754f, -1.278853f, -1.05093f
},
{ // coefficient B
-0.9368751f, -1.312676f, -0.7418582f, 0.2815928f, -0.5245326f, -0.2786331f
},
{ // coefficient C
-43.8298f, 2.863082f, -1.349589f, 0.5424052f, 0.787052f, 1.056344f
},
{ // coefficient D
44.24963f, -2.373727f, 1.563419f, -0.688545f, 0.3125067f, 1.053002f
},
{ // coefficient E
-0.00365253f, -0.00514498f, -0.003124219f, -0.1620001f, 0.7748105f, -4.047789f
},
{ // coefficient F
0.3094331f, 0.1711072f, 0.6967139f, 0.2980046f, -0.07788581f, 0.4432174f
},
{ // coefficient G
-2.810503e-06f, 0.09316041f, -0.3061887f, 0.4995571f, 0.003490956f, 0.1169077f
},
{ // coefficient H
1.904402f, 0.9309598f, 3.602731f, 0.7371203f, 1.283748f, 0.9532621f
},
{ // coefficient I
0.5861599f, 0.1791683f, 1.255669f, 0.2812466f, 0.813019f, 0.6806764f
},
{ // radiance
0.9264164f, 1.716454f, -1.597044f, 4.739725f, 45.07683f, 28.78915f
}
},
{ // turbidity 9
{ // coefficient A
-2.084927f, -2.328567f, -2.634632f, 0.06882616f, -1.430918f, -1.037956f
},
{ // coefficient B
-1.203954f, -1.238023f, -1.78946f, 0.5997821f, -0.6439041f, -0.3215402f
},
{ // coefficient C
-48.81638f, -1.891019f, -0.1370558f, 0.1535398f, 0.832598f, 0.9457349f
},
{ // coefficient D
49.2016f, 2.45152f, -0.3326435f, 1.375209f, -1.705612f, 3.178114f
},
{ // coefficient E
-0.002896045f, -0.005847581f, 0.002783737f, -0.1267285f, 0.7236426f, -4.152156f
},
{ // coefficient F
0.2882977f, 0.2084702f, 0.5239451f, 0.4239743f, -0.05567593f, 0.2230992f
},
{ // coefficient G
-3.073517e-06f, 0.0784813f, -0.2548881f, 0.4013122f, 0.06408718f, 0.1156198f
},
{ // coefficient H
1.702211f, 1.211048f, 2.896327f, 0.1794675f, 0.6836524f, 0.7606223f
},
{ // coefficient I
0.637418f, 0.08095008f, 1.324116f, 0.2395382f, 0.8388887f, 0.6656923f
},
{ // radiance
0.8595191f, 1.346034f, -0.02801895f, -0.6582906f, 50.17523f, 28.52953f
}
},
{ // turbidity 10
{ // coefficient A
-2.967314f, -2.123311f, -5.443597f, 1.029898f, -1.547574f, -0.9621043f
},
{ // coefficient B
-1.728778f, -1.175148f, -3.156344f, 0.5912521f, -0.7881604f, -0.1991406f
},
{ // coefficient C
-37.30988f, -13.14988f, 2.110838f, -0.3983531f, 1.020902f, 0.6531287f
},
{ // coefficient D
37.55578f, 13.86882f, -3.421556f, 3.286069f, -2.897069f, 3.925839f
},
{ // coefficient E
-0.002588835f, -0.007828537f, 0.0118189f, -0.09252065f, 0.521347f, -3.596904f
},
{ // coefficient F
0.2927966f, 0.1852026f, 0.1196951f, 1.331381f, -0.9242315f, 0.6317332f
},
{ // coefficient G
-3.935038e-06f, 0.05481038f, -0.1742902f, 0.2560642f, 0.1185594f, 0.1531334f
},
{ // coefficient H
1.592161f, 1.294309f, 2.404353f, 0.8001754f, -1.150721f, 1.457846f
},
{ // coefficient I
0.6868694f, 0.02428177f, 1.272805f, 0.3624178f, 0.7317211f, 0.6966285f
},
{ // radiance
0.7754116f, 0.7709245f, 2.200201f, -7.487661f, 54.36622f, 28.93432f
}
}
}
}
};

}

#endif	/* _SLG_ARHOSEKSKYMODELDATA_H */
