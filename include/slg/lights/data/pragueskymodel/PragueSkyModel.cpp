// Copyright 2022 Charles University
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <array>
#include <cassert>
#include <limits>
#include <cstring>

#include "PragueSkyModel.h"

/////////////////////////////////////////////////////////////////////////////////////
// Constants
/////////////////////////////////////////////////////////////////////////////////////

constexpr double PI               = 3.141592653589793;
constexpr double RAD_TO_DEG       = 180.0 / PI;
constexpr double PLANET_RADIUS    = 6378000.0;
constexpr double SAFETY_ALTITUDE  = 50.0;
constexpr double SUN_RADIUS       = 0.004654793; // = 0.2667 degrees
constexpr double DIST_TO_EDGE     = 1571524.413613; // Maximum distance to the edge of the atmosphere in the transmittance model
constexpr double ATMOSPHERE_WIDTH = 100000.0;
constexpr double SUN_RAD_START    = 280;
constexpr double SUN_RAD_STEP     = 1;
constexpr double SUN_RAD_TABLE[]  = {
    4022.51,  4592.96,  5058.84,  5552.97,  6024.88,  6443.47,  6836.20,  7175.78,  7380.47,  7437.44,
    7391.90,  7266.06,  7121.19,  7034.24,  7024.24,  7082.31,  7209.73,  7384.60,  7563.43,  7739.25,
    7905.70,  8025.18,  8096.11,  8202.27,  8337.15,  8602.30,  8965.52,  9316.67,  9565.64,  9814.45,
    9829.41,  10184.,   10262.6,  10375.7,  10276.,   10179.3,  10156.6,  10750.7,  11134.,   11463.6,
    11860.4,  12246.2,  12524.4,  12780.,   13187.4,  13632.4,  13985.9,  13658.3,  13377.4,  13358.3,
    13239.,   13119.8,  13096.2,  13184.,   13243.5,  13018.4,  12990.4,  13159.1,  13230.8,  13258.6,
    13209.9,  13343.2,  13404.8,  13305.4,  13496.3,  13979.1,  14153.8,  14188.4,  14122.7,  13825.4,
    14033.3,  13914.1,  13837.4,  14117.2,  13982.3,  13864.5,  14118.4,  14545.7,  15029.3,  15615.3,
    15923.5,  16134.8,  16574.5,  16509.,   16336.5,  16146.6,  15965.1,  15798.6,  15899.8,  16125.4,
    15854.3,  15986.7,  15739.7,  15319.1,  15121.5,  15220.2,  15041.2,  14917.7,  14487.8,  14011.,
    14165.7,  14189.5,  14540.7,  14797.5,  14641.5,  14761.6,  15153.7,  14791.8,  14907.6,  15667.4,
    16313.5,  16917.,   17570.5,  18758.1,  20250.6,  21048.1,  21626.1,  22811.6,  23577.2,  23982.6,
    24062.1,  23917.9,  23914.1,  23923.2,  24052.6,  24228.6,  24360.8,  24629.6,  24774.8,  24648.3,
    24666.5,  24938.6,  24926.3,  24693.1,  24613.5,  24631.7,  24569.8,  24391.5,  24245.7,  24084.4,
    23713.7,  22985.4,  22766.6,  22818.9,  22834.3,  22737.9,  22791.6,  23086.3,  23377.7,  23461.,
    23935.5,  24661.7,  25086.9,  25520.1,  25824.3,  26198.,   26350.2,  26375.4,  26731.2,  27250.4,
    27616.,   28145.3,  28405.9,  28406.8,  28466.2,  28521.5,  28783.8,  29025.1,  29082.6,  29081.3,
    29043.1,  28918.9,  28871.6,  29049.,   29152.5,  29163.2,  29143.4,  28962.7,  28847.9,  28854.,
    28808.7,  28624.1,  28544.2,  28461.4,  28411.1,  28478.,   28469.8,  28513.3,  28586.5,  28628.6,
    28751.5,  28948.9,  29051.,   29049.6,  29061.7,  28945.7,  28672.8,  28241.5,  27903.2,  27737.,
    27590.9,  27505.6,  27270.2,  27076.2,  26929.1,  27018.2,  27206.8,  27677.2,  27939.9,  27923.9,
    27899.2,  27725.4,  27608.4,  27599.4,  27614.6,  27432.4,  27460.4,  27392.4,  27272.,   27299.1,
    27266.8,  27386.5,  27595.9,  27586.9,  27504.8,  27480.6,  27329.8,  26968.4,  26676.3,  26344.7,
    26182.5,  26026.3,  25900.3,  25842.9,  25885.4,  25986.5,  26034.5,  26063.5,  26216.9,  26511.4,
    26672.7,  26828.5,  26901.8,  26861.5,  26865.4,  26774.2,  26855.8,  27087.1,  27181.3,  27183.1,
    27059.8,  26834.9,  26724.3,  26759.6,  26725.9,  26724.6,  26634.5,  26618.5,  26560.1,  26518.7,
    26595.3,  26703.2,  26712.7,  26733.9,  26744.3,  26764.4,  26753.2,  26692.7,  26682.7,  26588.1,
    26478.,   26433.7,  26380.7,  26372.9,  26343.3,  26274.7,  26162.3,  26160.5,  26210.,   26251.2,
    26297.9,  26228.9,  26222.3,  26269.7,  26295.6,  26317.9,  26357.5,  26376.1,  26342.4,  26303.5,
    26276.7,  26349.2,  26390.,   26371.6,  26346.7,  26327.6,  26274.2,  26247.3,  26228.7,  26152.1,
    25910.3,  25833.2,  25746.5,  25654.3,  25562.,   25458.8,  25438.,   25399.1,  25324.3,  25350.,
    25514.,   25464.9,  25398.5,  25295.2,  25270.2,  25268.4,  25240.6,  25184.9,  25149.6,  25123.9,
    25080.3,  25027.9,  25012.3,  24977.9,  24852.6,  24756.4,  24663.5,  24483.6,  24398.6,  24362.6,
    24325.1,  24341.7,  24288.7,  24284.2,  24257.3,  24178.8,  24097.6,  24175.6,  24175.7,  24139.7,
    24088.1,  23983.2,  23902.7,  23822.4,  23796.2,  23796.9,  23814.5,  23765.5,  23703.,   23642.,
    23592.6,  23552.,   23514.6,  23473.5,  23431.,   23389.3,  23340.,   23275.1,  23187.3,  23069.5,
    22967.,   22925.3,  22908.9,  22882.5,  22825.,   22715.4,  22535.5,  22267.1,  22029.4,  21941.6,
    21919.5,  21878.8,  21825.6,  21766.,   21728.9,  21743.2,  21827.1,  21998.7,  22159.4,  22210.,
    22187.2,  22127.2,  22056.2,  22000.2,  21945.9,  21880.2,  21817.1,  21770.3,  21724.3,  21663.2,
    21603.3,  21560.4,  21519.8,  21466.2,  21401.6,  21327.7,  21254.2,  21190.7,  21133.6,  21079.3,
    21024.,   20963.7,  20905.5,  20856.6,  20816.6,  20785.2,  20746.7,  20685.3,  20617.8,  20561.1,
    20500.4,  20421.2,  20333.4,  20247.,   20175.3,  20131.4,  20103.2,  20078.5,  20046.8,  19997.2,
    19952.9,  19937.2,  19930.8,  19914.4,  19880.8,  19823.,   19753.8,  19685.9,  19615.3,  19537.5,
    19456.8,  19377.6,  19309.4,  19261.9,  19228.,   19200.5,  19179.5,  19164.8,  19153.1,  19140.6,
    19129.2,  19120.6,  19104.5,  19070.6,  19023.9,  18969.3,  18911.4,  18855.,   18798.6,  18740.8,
    18672.7,  18585.2,  18501.,   18442.4,  18397.5,  18353.9,  18313.2,  18276.8,  18248.3,  18231.2,
    18224.,   18225.4,  18220.1,  18192.6,  18155.1,  18119.8,  18081.6,  18035.6,  17987.4,  17942.8,
    17901.7,  17864.2,  17831.1,  17802.9,  17771.5,  17728.6,  17669.7,  17590.1,  17509.5,  17447.4,
    17396.,   17347.4,  17300.3,  17253.2,  17206.1,  17159.,   17127.6,  17127.6,  17133.6,  17120.4,
    17097.2,  17073.3,  17043.7,  17003.4,  16966.3,  16946.3,  16930.9,  16907.7,  16882.7,  16862.,
    16837.8,  16802.1,  16759.2,  16713.6,  16661.8,  16600.8,  16542.6,  16499.4,  16458.7,  16408.,
    16360.6,  16329.5,  16307.4,  16286.7,  16264.9,  16239.6,  16207.8,  16166.8,  16118.2,  16064.,
    16011.2,  15966.9,  15931.9,  15906.9,  15889.1,  15875.5,  15861.2,  15841.3,  15813.1,  15774.2,
    15728.8,  15681.4,  15630.,   15572.9,  15516.5,  15467.2,  15423.,   15381.6,  15354.4,  15353.,
    15357.3,  15347.3,  15320.2,  15273.1,  15222.,   15183.1,  15149.6,  15114.6,  15076.8,  15034.6,
    14992.9,  14996.74, 14959.93, 14915.18, 14861.86, 14810.61, 14762.06, 14708.42, 14657.64, 14594.65,
    14508.80, 14422.96, 14344.89, 14271.90, 14219.06, 14189.07, 14159.08, 14128.61, 14100.36, 14071.01,
    14037.85, 13995.16, 13943.43, 13891.38, 13842.35, 13804.11, 13782.37, 13791.10, 13821.57, 13852.98,
    13877.58, 13898.05, 13900.43, 13880.75, 13859.81, 13837.43, 13810.93, 13783.01, 13757.62, 13751.43,
    13764.60, 13780.79, 13797.29, 13811.41, 13806.49, 13783.01, 13757.30, 13727.31, 13695.73, 13662.41,
    13626.87, 13593.54, 13564.51, 13535.31, 13506.11, 13477.71, 13445.66, 13407.57, 13367.90, 13327.28,
    13284.91, 13240.17, 13195.42, 13146.70, 13093.39, 13038.49, 12982.63, 12926.46, 12873.94, 12823.00,
    12770.64, 12720.02, 12670.51, 12623.06, 12579.74, 12536.11, 12488.98, 12438.84, 12384.73, 12327.60,
    12271.91, 12222.88, 12180.03, 12142.74, 12110.06, 12079.91, 12047.06, 12014.05, 11981.53, 11948.36,
    11916.63, 11883.94, 11847.76, 11813.48, 11782.22, 11754.14, 11731.61, 11711.45, 11688.29, 11664.01,
    11638.46, 11609.90, 11581.50, 11551.82, 11518.98, 11482.16, 11443.13, 11402.98, 11363.15, 11324.44,
    11287.62, 11253.98, 11222.41, 11193.68, 11167.03, 11144.34, 11124.34, 11104.67, 11084.51, 11064.36,
    11042.31, 11019.77, 10999.14, 10983.28, 10971.69, 10963.44, 10957.09, 10954.56, 10952.97, 10947.89,
    10940.27, 10933.45, 10925.52, 10917.11, 10912.66, 10911.24, 10909.49, 10908.54, 10907.75, 10909.01,
    10913.30, 10915.20, 10913.62, 10912.35, 10909.49, 10904.41, 10902.51, 10904.57, 10906.79, 10906.48,
    10904.25, 10898.07, 10887.12, 10872.36, 10856.49, 10839.20, 10822.53, 10804.45, 10783.98, 10763.03,
    10741.93, 10716.85, 10689.88, 10662.86, 10633.89, 10602.97, 10575.14, 10551.62, 10530.56, 10511.96,
    10495.82, 10480.23, 10463.32, 10445.07, 10425.49, 10404.57, 10383.53, 10363.57, 10344.69, 10326.89,
    10310.16, 10293.63, 10276.39, 10258.46, 10239.83, 10220.51, 10200.89, 10181.41, 10162.05, 10142.82,
    10123.71, 10104.16, 10083.60, 10062.02, 10039.42, 10015.81, 9991.44,  9966.56,  9941.17,  9915.27,
    9888.87,  9862.37,  9836.19,  9810.32,  9784.77,  9759.55,  9735.05,  9711.69,  9689.47,  9668.40,
    9648.47,  9628.83,  9608.61,  9587.82,  9566.46,  9544.54,  9522.70,  9501.63,  9481.32,  9461.77,
    9442.98,  9424.13,  9404.39,  9383.76,  9362.24,  9339.84,  9317.15,  9294.77,  9272.72,  9250.98,
    9229.56,  9208.10,  9186.27,  9164.05,  9141.46,  9118.48,  9095.66,  9073.54,  9052.12,  9031.40,
    9011.37,  8992.33,  8974.56,  8958.06,  8942.82,  8928.86,  8915.56,  8902.33,  8889.16,  8876.05,
    8863.01,  8850.03,  8837.11,  8824.26,  8811.47,  8798.74,  8785.76,  8772.21,  8758.09,  8743.40,
    8728.13,  8712.52,  8696.78,  8680.91,  8664.91,  8648.79,  8632.61,  8616.42,  8600.24,  8584.05,
    8567.87,  8551.78,  8535.88,  8520.17,  8504.65,  8489.32,  8474.06,  8458.73,  8443.34,  8427.88,
    8412.36,  8397.03,  8382.15,  8367.71,  8353.71,  8340.16,  8326.52,  8312.23,  8297.32,  8281.77,
    8265.58,  8248.95,  8232.07,  8214.93,  8197.54,  8179.90,  8162.35,  8145.24,  8128.58,  8112.36,
    8096.59,  8081.04,  8065.49,  8049.94,  8034.39,  8018.84,  8003.29,  7987.74,  7972.19,  7956.64,
    7941.08,  7925.95,  7911.63,  7898.15,  7885.48,  7873.65,  7862.06,  7850.16,  7837.94,  7825.41,
    7812.56,  7799.29,  7785.52,  7771.24,  7756.45,  7741.15,  7725.73,  7710.56,  7695.64,  7680.98,
    7666.57,  7652.29,  7638.01,  7623.73,  7609.45,  7595.17,  7580.95,  7566.86,  7552.89,  7539.06,
    7525.35,  7511.22,  7496.15,  7480.12,  7463.14,  7445.21,  7427.19,  7409.92,  7393.42,  7377.68,
    7362.70,  7348.26,  7334.14,  7320.33,  7306.85,  7293.68,  7280.92,  7268.67,  7256.93,  7245.69,
    7234.96,  7224.14,  7212.62,  7200.40,  7187.49,  7173.87,  7160.26,  7147.34,  7135.12,  7123.60,
    7112.78,  7102.47,  7092.47,  7082.79,  7073.43,  7064.38,  7055.31,  7045.85,  7036.01,  7025.79,
    7015.19,  7004.69,  6994.76,  6985.39,  6976.60,  6968.38,  6960.29,  6951.88,  6943.15,  6934.11,
    6924.75,  6915.54,  6906.97,  6899.04,  6891.74,  6885.08,  6878.54,  6871.62,  6864.32,  6856.64,
    6848.58,  6839.38,  6828.27,  6815.26,  6800.34,  6783.52,  6766.16,  6749.63,  6733.92,  6719.04,
    6704.98,  6691.36,  6677.81,  6664.32,  6650.90,  6637.54,  6624.56,  6612.28,  6600.69,  6589.81,
    6579.62,  6569.43,  6558.55,  6546.96,  6534.68,  6521.70,  6508.44,  6495.30,  6482.29,  6469.40,
    6456.64,  6444.46,  6433.29,  6423.13,  6413.99,  6405.87,  6397.87,  6389.11,  6379.59,  6369.31,
    6358.26,  6347.19,  6336.81,  6327.13,  6318.15,  6309.87,  6302.25,  6295.27,  6288.92,  6283.21,
    6278.13,  6273.24,  6268.10,  6262.71,  6257.06,  6251.15,  6245.00,  6238.59,  6231.92,  6225.00,
    6217.83,  6210.09,  6201.46,  6191.94,  6181.53,  6170.23,  6158.90,  6148.39,  6138.71,  6129.86,
    6121.83,  6113.96,  6105.58,  6096.70,  6087.30,  6077.40,  6066.93,  6055.82,  6044.08,  6031.70,
    6018.69,  6005.42,  5992.29,  5979.27,  5966.39,  5953.63,  5940.81,  5927.74,  5914.41,  5900.82,
    5886.99,  5873.09,  5859.31,  5845.67,  5832.15,  5818.75,  5805.58,  5792.73,  5780.20,  5767.98,
    5756.08,  5744.30,  5732.47,  5720.56,  5708.60,  5696.57,  5684.26,  5671.44,  5658.11,  5644.27,
    5629.93,  5615.46,  5601.24,  5587.27,  5573.56,  5560.11,  5546.94,  5534.08,  5521.55,  5509.33,
    5497.43,  5485.72,  5474.07,  5462.49,  5450.97,  5439.51,  5428.15,  5416.92,  5405.81,  5394.83,
    5383.97,  5373.12,  5362.14,  5351.03,  5339.80,  5328.44,  5317.20,  5306.35,  5295.88,  5285.78,
    5276.07,  5266.58,  5257.16,  5247.80,  5238.50,  5229.26,  5220.09,  5210.98,  5201.94,  5192.96,
    5184.04,  5175.28,  5166.78,  5158.52,  5150.53,  5142.78,  5135.13,  5127.42,  5119.65,  5111.81,
    5103.91,  5095.97,  5088.04,  5080.10,  5072.17,  5064.24,  5056.59,  5049.51,  5043.01,  5037.07,
    5031.71,  5026.28,  5020.16,  5013.33,  5005.81,  4997.59,  4989.02,  4980.45,  4971.89,  4963.32,
    4954.75,  4946.08,  4937.23,  4928.19,  4918.95,  4909.52,  4899.85,  4889.85,  4879.53,  4868.90,
    4857.95,  4847.10,  4836.75,  4826.92,  4817.59,  4808.76,  4799.81,  4790.10,  4779.63,  4768.40,
    4756.40,  4744.40,  4733.17,  4722.70,  4712.99,  4704.04,  4695.31,  4686.26,  4676.90,  4667.22,
    4657.23,  4646.91,  4636.28,  4625.33,  4614.06,  4602.48,  4590.96,  4579.89,  4569.25,  4559.07,
    4549.32,  4539.87,  4530.54,  4521.33,  4512.26,  4503.31,  4494.39,  4485.41,  4476.36,  4467.26,
    4458.08,  4449.32,  4441.45,  4434.47,  4428.38,  4423.17,  4418.16,  4412.64,  4406.61,  4400.07,
    4393.03,  4386.11,  4379.95,  4374.55,  4369.92,  4366.05,  4362.21,  4357.67,  4352.43,  4346.50,
    4339.87,  4332.92,  4326.03,  4319.21,  4312.45,  4305.75,  4299.09,  4292.42,  4285.76,  4279.09,
    4272.43,  4265.76,  4259.10,  4252.44,  4245.77,  4239.11,  4232.70,  4226.79,  4221.40,  4216.51,
    4212.13,  4207.91,  4203.50,  4198.90,  4194.11,  4189.12,  4183.63,  4177.32,  4170.18,  4162.21,
    4153.42,  4144.34,  4135.52,  4126.95,  4118.64,  4110.58,  4102.74,  4095.09,  4087.63,  4080.36,
    4073.29,  4066.31,  4059.32,  4052.34,  4045.36,  4038.38,  4031.52,  4024.92,  4018.57,  4012.48,
    4006.64,  4000.83,  3994.84,  3988.65,  3982.27,  3975.70,  3968.97,  3962.12,  3955.13,  3948.03,
    3940.79,  3933.27,  3925.30,  3916.89,  3908.04,  3898.74,  3889.44,  3880.59,  3872.18,  3864.21,
    3856.69,  3849.52,  3842.60,  3835.94,  3829.52,  3823.37,  3817.21,  3810.80,  3804.14,  3797.22,
    3790.05,  3782.59,  3774.81,  3766.72,  3758.31,  3749.58,  3740.73,  3731.94,  3723.21,  3714.55,
    3705.95,  3697.44,  3689.06,  3680.81,  3672.69,  3664.69,  3656.69,  3648.57,  3640.32,  3631.94,
    3623.43,  3615.12,  3607.31,  3600.01,  3593.22,  3586.94,  3580.97,  3575.13,  3569.42,  3563.83,
    3558.37,  3552.92,  3547.33,  3541.62,  3535.78,  3529.81,  3524.04,  3518.77,  3514.01,  3509.75,
    3506.01,  3502.52,  3499.03,  3495.54,  3492.05,  3488.56,  3485.13,  3481.83,  3478.65,  3475.61,
    3472.69,  3469.74,  3466.59,  3463.26,  3459.74,  3456.03,  3452.22,  3448.41,  3444.60,  3440.79,
    3436.98,  3433.30,  3429.88,  3426.70,  3423.78,  3421.12,  3418.51,  3415.79,  3412.93,  3409.95,
    3406.84,  3403.66,  3400.49,  3397.32,  3394.14,  3390.97,  3387.76,  3384.49,  3381.16,  3377.77,
    3374.31,  3370.66,  3366.69,  3362.41,  3357.80,  3352.88,  3347.65,  3342.09,  3336.22,  3330.04,
    3323.53,  3317.02,  3310.84,  3304.96,  3299.41,  3294.17,  3289.13,  3284.15,  3279.23,  3274.37,
    3269.58,  3264.79,  3259.93,  3255.01,  3250.03,  3244.98,  3239.78,  3234.32,  3228.61,  3222.64,
    3216.42,  3210.17,  3204.11,  3198.24,  3192.56,  3187.07,  3181.54,  3175.77,  3169.74,  3163.45,
    3156.92,  3150.09,  3142.95,  3135.49,  3127.72,  3119.63,  3111.69,  3104.39,  3097.73,  3091.70,
    3086.30,  3081.26,  3076.28,  3071.36,  3066.50,  3061.71,  3056.63,  3050.92,  3044.57,  3037.59,
    3029.97,  3021.91,  3013.60,  3005.03,  2996.21,  2987.13,  2977.89,  2968.60,  2959.23,  2949.81,
    2940.32,  2931.02,  2922.17,  2913.76,  2905.79,  2898.27,  2890.69,  2882.53,  2873.80,  2864.50,
    2854.63,  2844.70,  2835.21,  2826.17,  2817.57,  2809.41,  2801.82,  2794.94,  2788.75,  2783.26,
    2778.47,  2773.58,  2767.80,  2761.14,  2753.59,  2745.14,  2736.42,  2728.01,  2719.91,  2712.14,
    2704.68,  2697.51,  2690.59,  2683.93,  2677.52,  2671.36,  2665.39,  2659.55,  2653.84,  2648.26,
    2642.80,  2637.37,  2631.88,  2626.33,  2620.71,  2615.03,  2609.22,  2603.22,  2597.03,  2590.66,
    2584.09,  2577.58,  2571.39,  2565.52,  2559.97,  2554.73,  2549.72,  2544.83,  2540.07,  2535.43,
    2530.93,  2526.29,  2521.28,  2515.89,  2510.11,  2503.95,  2497.73,  2491.77,  2486.05,  2480.60,
    2475.39,  2470.12,  2464.47,  2458.44,  2452.03,  2445.24,  2438.42,  2431.91,  2425.72,  2419.85,
    2414.30,  2409.00,  2403.89,  2398.97,  2394.24,  2389.70,  2385.23,  2380.69,  2376.09,  2371.42,
    2366.70,  2361.97,  2357.30,  2352.70,  2348.16,  2343.69,  2339.09,  2334.17,  2328.93,  2323.38,
    2317.51,  2311.44,  2305.32,  2299.13,  2292.88,  2286.56,  2280.28,  2274.12,  2268.09,  2262.19,
    2256.41,  2250.64,  2244.74,  2238.71,  2232.55,  2226.26,  2220.08,  2214.21,  2208.65,  2203.42,
    2198.50,  2193.67,  2188.72,  2183.64,  2178.44,  2173.11,  2167.74,  2162.44,  2157.21,  2152.03,
    2146.93,  2141.69,  2136.14,  2130.26,  2124.08,  2117.57,  2110.91,  2104.24,  2097.58,  2090.91,
    2084.25,  2077.93,  2072.31,  2067.40,  2063.17,  2059.65,  2056.32,  2052.67,  2048.70,  2044.42,
    2039.82,  2035.09,  2030.42,  2025.82,  2021.28,  2016.81,  2012.49,  2008.43,  2004.62,  2001.07,
    1997.77,  1994.53,  1991.17,  1987.68,  1984.06,  1980.31,  1976.66,  1973.33,  1970.32,  1967.62,
    1965.24,  1963.08,  1961.05,  1959.14,  1957.37,  1955.72,  1954.32,  1953.31,  1952.67,  1952.42,
    1952.54,  1952.80,  1952.92,  1952.92,  1952.80,  1952.54,  1952.00,  1951.02,  1949.59,  1947.72,
    1945.40,  1942.93,  1940.58,  1938.36,  1936.26,  1934.30,  1932.39,  1930.49,  1928.58,  1926.68,
    1924.77,  1922.78,  1920.59,  1918.21,  1915.64,  1912.87,  1910.11,  1907.54,  1905.16,  1902.97,
    1900.97,  1898.94,  1896.66,  1894.12,  1891.33,  1888.28,  1884.91,  1881.17,  1877.04,  1872.54,
    1867.65,  1862.76,  1858.26,  1854.13,  1850.39,  1847.02,  1843.88,  1840.80,  1837.79,  1834.84,
    1831.95,  1829.16,  1826.49,  1823.95,  1821.54,  1819.25,  1817.16,  1815.32,  1813.73,  1812.40,
    1811.32,  1810.27,  1809.03,  1807.61,  1805.99,  1804.18,  1802.27,  1800.37,  1798.47,  1796.56,
    1794.66,  1792.75,  1790.85,  1788.95,  1787.04,  1785.14,  1783.33,  1781.71,  1780.28,  1779.04,
    1778.00,  1776.95,  1775.71,  1774.28,  1772.67,  1770.86,  1768.76,  1766.29,  1763.43,  1760.19,
    1756.58,  1752.77,  1748.96,  1745.15,  1741.34,  1737.53,  1733.63,  1729.54,  1725.25,  1720.78,
    1716.11,  1711.13,  1705.70,  1699.83,  1693.52,  1686.76,  1680.14,  1674.25,  1669.10,  1664.67,
    1660.97,  1657.64,  1654.31,  1650.97,  1647.64,  1644.31,  1640.95,  1637.52,  1634.03,  1630.47,
    1626.86,  1623.21,  1619.56,  1615.91,  1612.26,  1608.61,  1605.01,  1601.50,  1598.09,  1594.77,
    1591.55,  1588.38,  1585.20,  1582.03,  1578.85,  1575.68,  1572.43,  1569.02,  1565.45,  1561.72,
    1557.83,  1553.86,  1549.90,  1545.93,  1541.96,  1538.00,  1534.06,  1530.19,  1526.38,  1522.64,
    1518.95,  1515.30,  1511.65,  1508.00,  1504.36,  1500.71,  1496.99,  1493.15,  1489.19,  1485.09,
    1480.87,  1476.59,  1472.30,  1468.02,  1463.73,  1459.45,  1455.30,  1451.41,  1447.79,  1444.43,
    1441.33,  1438.37,  1435.41,  1432.45,  1429.49,  1426.52,  1423.56,  1420.60,  1417.64,  1414.68,
    1411.71,  1408.74,  1405.75,  1402.73,  1399.70,  1396.64,  1393.57,  1390.50,  1387.44,  1384.37,
    1381.30,  1378.23,  1375.16,  1372.10,  1369.03,  1365.96,  1362.91,  1359.91,  1356.95,  1354.03,
    1351.15,  1348.29,  1345.44,  1342.58,  1339.73,  1336.87,  1334.01,  1331.16,  1328.30,  1325.44,
    1322.59,  1319.71,  1316.79,  1313.83,  1310.83,  1307.78,  1304.71,  1301.64,  1298.58,  1295.51,
    1292.44,  1289.37,  1286.30,  1283.24,  1280.17,  1277.10,  1274.05,  1271.05,  1268.09,  1265.17,
    1262.29,  1259.43,  1256.58,  1253.72,  1250.87,  1248.01,  1245.15,  1242.30,  1239.44,  1236.58,
    1233.73,  1230.88,  1228.06,  1225.25,  1222.47,  1219.71,  1216.96,  1214.21,  1211.46,  1208.71,
    1205.96,  1203.21,  1200.46,  1197.71,  1194.96,  1192.21,  1189.49,  1186.83,  1184.24,  1181.71,
    1179.25,  1176.82,  1174.38,  1171.95,  1169.52,  1167.08,  1164.65,  1162.22,  1159.78,  1157.35,
    1154.92,  1152.51,  1150.14,  1147.81,  1145.52,  1143.28,  1141.06,  1138.84,  1136.62,  1134.40,
    1132.17,  1129.95,  1127.73,  1125.51,  1123.29,  1121.07,  1118.90,  1116.83,  1114.88,  1113.03,
    1111.28,  1109.59,  1107.90,  1106.20,  1104.51,  1102.82,  1101.13,  1099.43,  1097.74,  1096.05,
    1094.36,  1092.66,  1090.97,  1089.28,  1087.59,  1085.89,  1084.20,  1082.51,  1080.81,  1079.12,
    1077.43,  1075.74,  1074.04,  1072.35,  1070.66,  1068.97,  1067.28,  1065.62,  1063.98,  1062.37,
    1060.77,  1059.18,  1057.59,  1056.01,  1054.42,  1052.83,  1051.25,  1049.66,  1048.07,  1046.49,
    1044.90,  1043.26,  1041.50,  1039.64,  1037.66,  1035.58,  1033.44,  1031.29,  1029.15,  1027.01,
    1024.87,  1022.73,  1020.58,  1018.44,  1016.30,  1014.16,  1012.01,  1009.87,  1007.73,  1005.59,
    1003.45,  1001.34,  999.30,   997.33,   995.43,   993.59,   991.80,   990.00,   988.20,   986.40,
    984.60,   982.80,   981.01,   979.21,   977.41,   975.61,   973.93,   972.48,   971.26,   970.28,
    969.53,   968.89,   968.26,   967.62,   966.99,   966.35,   965.72,   965.09,   964.45,   963.82,
    963.18,   962.55,   961.91,   961.28,   960.64,   960.01,   959.28,   958.36,   957.25,   955.95,
    954.45,   952.87,   951.28,   949.69,   948.11,   946.52,   944.93,   943.35,   941.76,   940.17,
    938.59,   937.00,   935.41,   933.83,   932.24,   930.65,   929.06,   927.45,   925.82,   924.18,
    922.52,   920.85,   919.19,   917.52,   915.85,   914.19,   912.52,   910.86,   909.19,   907.52,
    905.86,   904.19,   902.53,   900.86,   899.19,   897.53,   895.91,   894.39,   892.96,   891.62,
    890.39,   889.20,   888.01,   886.82,   885.63,   884.44,   883.25,   882.06,   880.87,   879.68,
    878.49,   877.30,   876.11,   874.92,   873.73,   872.54,   871.31,   870.00,   868.61,   867.14,
    865.59,   864.01,   862.42,   860.83,   859.25,   857.66,   856.07,   854.49,   852.90,   851.31,
    849.73,   848.14,   846.55,   844.97,   843.38,   841.79,   840.24,   838.74,   837.32,   835.95,
    834.65,   833.38,   832.11,   830.84,   829.57,   828.30,   827.03,   825.76,   824.50,   823.23,
    821.96,   820.69,   819.42,   818.15,   816.88,   815.61,   814.37,   813.20,   812.09,   811.04,
    810.06,   809.10,   808.15,   807.20,   806.25,   805.30,   804.34,   803.39,   802.44,   801.49,
    800.53,   799.58,   798.63,   797.68,   796.73,   795.77,   794.87,   794.06,   793.35,   792.73,
    792.20,   791.73,   791.25,   790.78,   790.30,   789.82,   789.35,   788.87,   788.40,   787.92,
    787.44,   786.97,   786.49,   786.02,   785.54,   785.06,   784.61,   784.19,   783.81,   783.48,
    783.18,   782.90,   782.62,   782.35,   782.07,   781.79,   781.51,   781.24,   780.96,   780.68,
    780.40,   780.12,   779.85,   779.57,   779.29,   779.01,   778.68,   778.23,   777.66,   776.98,
    776.18,   775.32,   774.46,   773.61,   772.75,   771.89,   771.04,   770.18,   769.32,   768.47,
    767.61,   766.75,   765.90,   765.04,   764.18,   763.32,   762.47,   761.61,   760.75,   759.90,
    759.04,   758.25,   757.59,   757.07,   756.68,   756.42,   756.23,   756.04,   755.85,   755.66,
    755.47,   755.28,   755.09,   754.90,   754.71,   754.52,   754.33,   754.14,   753.95,   753.76,
    753.57,   753.38,   753.19,   752.99,   752.80,   752.61,   752.43,   752.25,   752.08,   751.91,
    751.76,   751.60,   751.45,   751.30,   751.15,   751.00,   750.84,   750.69,   750.54,   750.39,
    750.2
};
constexpr double SUN_RAD_END = SUN_RAD_START + SUN_RAD_STEP * std::size(SUN_RAD_TABLE);


/////////////////////////////////////////////////////////////////////////////////////
// Conversion functions
/////////////////////////////////////////////////////////////////////////////////////

double radiansToDegrees(const double radians) {
    return radians * RAD_TO_DEG;
}

typedef unsigned short     uint16;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

double doubleFromHalf(const uint16 half) {
    uint32 hi  = uint32(half & 0x8000) << 16;
    uint16 abs = half & 0x7FFF;
    if (abs) {
        hi |= 0x3F000000 << uint16(abs >= 0x7C00);
        for (; abs < 0x400; abs <<= 1, hi -= 0x100000)
            ;
        hi += uint32(abs) << 10;
    }
    const uint64 dbits = uint64(hi) << 32;
    double out;
    std::memcpy(&out, &dbits, sizeof(double));
    return out;
}


/////////////////////////////////////////////////////////////////////////////////////
// Vector3 operations
/////////////////////////////////////////////////////////////////////////////////////

double dot(const PragueSkyModel::Vector3& v1, const PragueSkyModel::Vector3& v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

double magnitude(const PragueSkyModel::Vector3& vector) {
    return std::sqrt(dot(vector, vector));
}

PragueSkyModel::Vector3 normalize(const PragueSkyModel::Vector3& vector) {
    return vector / magnitude(vector);
}


/////////////////////////////////////////////////////////////////////////////////////
// Helper functions
/////////////////////////////////////////////////////////////////////////////////////

double lerpPrague(const double from, const double to, const double factor) {
    return (1.0 - factor) * from + factor * to;
}

double nonlerp(const double a, const double b, const double w, const double p) {
    const double c1 = pow(a, p);
    const double c2 = pow(b, p);
    return ((pow(w, p) - c1) / (c2 - c1));
}


/////////////////////////////////////////////////////////////////////////////////////
// Data reading
/////////////////////////////////////////////////////////////////////////////////////

void PragueSkyModel::readRadiance(FILE* handle, const double singleVisibility) {
    // Read metadata.

    // Structure of the metadata part of the data file:
    // visibilityCount      (1 * int), visibilities         (visibilityCount * double),
    // albedoCount          (1 * int), albedos                  (albedoCount * double),
    // altitudeCount        (1 * int), altitudes              (altitudeCount * double),
    // elevationCount       (1 * int), elevations            (elevationCount * double),
    // channels             (1 * int), channelStart                       (1 * double), channelWidth (1 * double), 
    // rankRad              (1 * int), 
    // sunBreaksCountRad    (1 * int), sunBreaksRad       (sunBreaksCountRad * double), 
    // zenithBreaksCountRad (1 * int), zenithBreaksRad (zenithBreaksCountRad * double), 
    // emphBreaksCountRad   (1 * int), emphBreaksRad     (emphBreaksCountRad * double)

    size_t valsRead;

    int visibilityCount = 0;
    valsRead            = fread(&visibilityCount, sizeof(int), 1, handle);
    if (valsRead != 1 || visibilityCount < 1)
        throw DatasetReadException("visibilityCountRad");

    std::vector<double> visibilitiesRadInFile;
    visibilitiesRadInFile.resize(visibilityCount);
    valsRead = fread(visibilitiesRadInFile.data(), sizeof(double), visibilityCount, handle);
    if (valsRead != visibilityCount)
        throw DatasetReadException("visibilitesRad");

    int skippedVisibilities = 0;
    if (singleVisibility <= 0.0 || visibilityCount <= 1) {
        // If the given single visibility value is not valid or there is just one visibility present (so there
        // is nothing to save), all visibilities present are loaded.
        visibilitiesRad.resize(visibilityCount);
        visibilitiesRad = visibilitiesRadInFile;
    } else {
        // Otherwise, only the two visibilities closest to the single visibility value are loaded and the rest
        // is skipped.
        if (singleVisibility <= visibilitiesRadInFile.front()) {
            visibilitiesRad.resize(1);
            visibilitiesRad[0] = visibilitiesRadInFile.front();
        } else if (singleVisibility >= visibilitiesRadInFile.back()) {
            visibilitiesRad.resize(1);
            visibilitiesRad[0] = visibilitiesRadInFile.back();
            skippedVisibilities = visibilitiesRadInFile.size() - 1;
        }
        else {
            int visIdx = 0;
            while (singleVisibility >= visibilitiesRadInFile[visIdx]) {
                ++visIdx;
            }
            visibilitiesRad.resize(2);
            visibilitiesRad[0] = visibilitiesRadInFile[visIdx - 1];
            visibilitiesRad[1] = visibilitiesRadInFile[visIdx];
            skippedVisibilities = visIdx - 1;
        }        
    }

    int albedoCount = 0;
    valsRead        = fread(&albedoCount, sizeof(int), 1, handle);
    if (valsRead != 1 || albedoCount < 1)
        throw DatasetReadException("albedoCountRad");

    albedosRad.resize(albedoCount);
    valsRead = fread(albedosRad.data(), sizeof(double), albedoCount, handle);
    if (valsRead != albedoCount)
        throw DatasetReadException("albedosRad");

    int altitudeCount = 0;
    valsRead          = fread(&altitudeCount, sizeof(int), 1, handle);
    if (valsRead != 1 || altitudeCount < 1)
        throw DatasetReadException("altitudeCountRad");

    altitudesRad.resize(altitudeCount);
    valsRead = fread(altitudesRad.data(), sizeof(double), altitudeCount, handle);
    if (valsRead != altitudeCount)
        throw DatasetReadException("altitudesRad");

    int elevationCount = 0;
    valsRead           = fread(&elevationCount, sizeof(int), 1, handle);
    if (valsRead != 1 || elevationCount < 1)
        throw DatasetReadException(" elevationCountRad");

    elevationsRad.resize(elevationCount);
    valsRead = fread(elevationsRad.data(), sizeof(double), elevationCount, handle);
    if (valsRead != elevationCount)
        throw DatasetReadException("elevationsRad");

    valsRead = fread(&channels, sizeof(int), 1, handle);
    if (valsRead != 1 || channels < 1)
        throw DatasetReadException("channels");

    valsRead = fread(&channelStart, sizeof(double), 1, handle);
    if (valsRead != 1 || channelStart < 0)
        throw DatasetReadException("channelStart");

    valsRead = fread(&channelWidth, sizeof(double), 1, handle);
    if (valsRead != 1 || channelWidth <= 0)
        throw DatasetReadException("channelWidth");

    totalConfigs =
        channels * elevationsRad.size() * altitudesRad.size() * albedosRad.size() * visibilitiesRad.size();

    skippedConfigsBegin =
        channels * elevationsRad.size() * altitudesRad.size() * albedosRad.size() * skippedVisibilities;

    skippedConfigsEnd = channels * elevationsRad.size() * altitudesRad.size() * albedosRad.size() *
                        (visibilitiesRadInFile.size() - skippedVisibilities - visibilitiesRad.size());

    valsRead = fread(&metadataRad.rank, sizeof(int), 1, handle);
    if (valsRead != 1 || metadataRad.rank < 1)
        throw DatasetReadException("rankRad");

    int sunBreaksCount = 0;
    valsRead           = fread(&sunBreaksCount, sizeof(int), 1, handle);
    if (valsRead != 1 || sunBreaksCount < 2)
        throw DatasetReadException("sunBreaksCountRad");

    metadataRad.sunBreaks.resize(sunBreaksCount);
    valsRead = fread(metadataRad.sunBreaks.data(), sizeof(double), sunBreaksCount, handle);
    if (valsRead != sunBreaksCount)
        throw DatasetReadException("sunBreaksRad");

    int zenitBreaksCount = 0;
    valsRead             = fread(&zenitBreaksCount, sizeof(int), 1, handle);
    if (valsRead != 1 || zenitBreaksCount < 2)
        throw DatasetReadException("zenitBreaksCountRad");

    metadataRad.zenithBreaks.resize(zenitBreaksCount);
    valsRead = fread(metadataRad.zenithBreaks.data(), sizeof(double), zenitBreaksCount, handle);
    if (valsRead != zenitBreaksCount)
        throw DatasetReadException("zenithBreaksRad");

    int emphBreaksCount = 0;
    valsRead            = fread(&emphBreaksCount, sizeof(int), 1, handle);
    if (valsRead != 1 || emphBreaksCount < 2)
        throw DatasetReadException("emphBreaksCountRad");

    metadataRad.emphBreaks.resize(emphBreaksCount);
    valsRead = fread(metadataRad.emphBreaks.data(), sizeof(double), emphBreaksCount, handle);
    if (valsRead != emphBreaksCount)
        throw DatasetReadException("emphBreaksRad");

    // Calculate offsets and strides.

    metadataRad.sunOffset = 0;
    metadataRad.sunStride = metadataRad.sunBreaks.size() + metadataRad.zenithBreaks.size();

    metadataRad.zenithOffset = metadataRad.sunOffset + metadataRad.sunBreaks.size();
    metadataRad.zenithStride = metadataRad.sunStride;

    metadataRad.emphOffset = metadataRad.sunOffset + metadataRad.rank * metadataRad.sunStride;

    metadataRad.totalCoefsSingleConfig =
        metadataRad.emphOffset + metadataRad.emphBreaks.size(); // this is for one specific configuration
    metadataRad.totalCoefsAllConfigs = metadataRad.totalCoefsSingleConfig * totalConfigs;

    // Read data.

    // Structure of the data part of the data file:
    // [[[[[[ sunCoefsRad       (sunBreaksCountRad * half), zenithScale (1 * double), 
    //        zenithCoefsRad (zenithBreaksCountRad * half) ] * rankRad, 
    //        emphCoefsRad     (emphBreaksCountRad * half) ]
    //  * channels ] * elevationCount ] * altitudeCount ] * albedoCount ] * visibilityCount

    int offset = 0;
    dataRad.resize(metadataRad.totalCoefsAllConfigs);

    std::vector<uint16> radianceTemp;
    radianceTemp.resize(std::max(metadataRad.sunBreaks.size(),
                                 std::max(metadataRad.zenithBreaks.size(), metadataRad.emphBreaks.size())));

    size_t oneConfigByteCount =
        ((metadataRad.sunBreaks.size() + metadataRad.zenithBreaks.size()) * sizeof(uint16) + sizeof(double)) *
            metadataRad.rank +
        metadataRad.emphBreaks.size() * sizeof(uint16);

    // If a single visibility was requested, skip all configurations from the beginning till those needed for
    // the requested visibility.
    fseek(handle, oneConfigByteCount * skippedConfigsBegin, SEEK_CUR);

    // Read configurations needed for the requested visibility (or all if none requested).
    for (int con = 0; con < totalConfigs; ++con) {
        for (int r = 0; r < metadataRad.rank; ++r) {
            // Read sun params.
            valsRead = fread(radianceTemp.data(), sizeof(uint16), metadataRad.sunBreaks.size(), handle);
            if (valsRead != metadataRad.sunBreaks.size())
                throw DatasetReadException("sunCoefsRad");

            // Unpack sun params from half.
            for (int i = 0; i < metadataRad.sunBreaks.size(); ++i) {
                dataRad[offset++] = float(doubleFromHalf(radianceTemp[i]));
            }

            // Read scaling factor for zenith params.
            double zenithScale;
            valsRead = fread(&zenithScale, sizeof(double), 1, handle);
            if (valsRead != 1)
                throw DatasetReadException("zenithScaleRad");

            // Read zenith params.
            valsRead = fread(radianceTemp.data(), sizeof(uint16), metadataRad.zenithBreaks.size(), handle);
            if (valsRead != metadataRad.zenithBreaks.size())
                throw DatasetReadException("zenithCoefsRad");

            // Unpack zenith params from half (these need additional rescaling).
            for (int i = 0; i < metadataRad.zenithBreaks.size(); ++i) {
                dataRad[offset++] = float(doubleFromHalf(radianceTemp[i]) / zenithScale);
            }
        }

        // Read emphasize params.
        valsRead = fread(radianceTemp.data(), sizeof(uint16), metadataRad.emphBreaks.size(), handle);
        if (valsRead != metadataRad.emphBreaks.size())
            throw DatasetReadException("emphCoefsRad");

        // Unpack emphasize params from half.
        for (int i = 0; i < metadataRad.emphBreaks.size(); ++i) {
            dataRad[offset++] = float(doubleFromHalf(radianceTemp[i]));
        }
    }

    // Skip remaining configurations till the end.
    fseek(handle, oneConfigByteCount * skippedConfigsEnd, SEEK_CUR);
}

void PragueSkyModel::readTransmittance(FILE* handle) {
    // Read metadata

    size_t valsRead;

    valsRead = fread(&dDim, sizeof(int), 1, handle);
    if (valsRead != 1 || dDim < 1)
        throw DatasetReadException("dDim");

    valsRead = fread(&aDim, sizeof(int), 1, handle);
	if (valsRead != 1 || aDim < 1)
        throw DatasetReadException("aDim");

    int visibilitiesCount = 0;
    valsRead = fread(&visibilitiesCount, sizeof(int), 1, handle);
    if (valsRead != 1 || visibilitiesCount < 1)
        throw DatasetReadException("visibilitiesCountTrans");

    int altitudesCount = 0;
    valsRead = fread(&altitudesCount, sizeof(int), 1, handle);
    if (valsRead != 1 || altitudesCount < 1)
        throw DatasetReadException("altitudesCountTrans");

    valsRead = fread(&rankTrans, sizeof(int), 1, handle);
    if (valsRead != 1 || rankTrans < 1)
        throw DatasetReadException("rankTrans");

    std::vector<float> temp;
    temp.resize(std::max(altitudesCount, visibilitiesCount));

    altitudesTrans.resize(altitudesCount);
    valsRead = fread(temp.data(), sizeof(float), altitudesCount, handle);
    if (valsRead != altitudesCount)
        throw DatasetReadException("altitudesTrans");
    for (int i = 0; i < altitudesCount; i++) {
        altitudesTrans[i] = double(temp[i]);
    }

    visibilitiesTrans.resize(visibilitiesCount);
    valsRead = fread(temp.data(), sizeof(float), visibilitiesCount, handle);
    if (valsRead != visibilitiesCount)
        throw DatasetReadException("visibilitiesTrans");
    for (int i = 0; i < visibilitiesCount; i++) {
        visibilitiesTrans[i] = double(temp[i]);
    }

    const size_t totalCoefsU = dDim * aDim * rankTrans * altitudesTrans.size();
    const size_t totalCoefsV = visibilitiesTrans.size() * rankTrans * channels * altitudesTrans.size();

    // Read data

    dataTransU.resize(totalCoefsU);
    valsRead = fread(dataTransU.data(), sizeof(float), totalCoefsU, handle);
    if (valsRead != totalCoefsU)
        throw DatasetReadException("datasetTransU");

    dataTransV.resize(totalCoefsV);
    valsRead = fread(dataTransV.data(), sizeof(float), totalCoefsV, handle);
    if (valsRead != totalCoefsV)
        throw DatasetReadException("datasetTransV");
}

void PragueSkyModel::readPolarisation(FILE* handle) {
    // Read metadata.

    // Structure of the metadata part of the data file:
    // rankPol              (1 * int), 
    // sunBreaksCountPol    (1 * int), sunBreaksPol    (sunBreaksCountPol * double),
    // zenithBreaksCountPol (1 * int), zenithBreaksPol (zenithBreaksCountPol * double), 

    size_t valsRead;

    valsRead = fread(&metadataPol.rank, sizeof(int), 1, handle);
    if (valsRead != 1) {
        // Polarisation dataset not present
        metadataPol.rank = 0;
        return;
    }

    int sunBreaksCount = 0;
    valsRead           = fread(&sunBreaksCount, sizeof(int), 1, handle);
    if (valsRead != 1 || sunBreaksCount < 1)
        throw DatasetReadException("sunBreaksCountPol");

    metadataPol.sunBreaks.resize(sunBreaksCount);
    valsRead = fread(metadataPol.sunBreaks.data(), sizeof(double), sunBreaksCount, handle);
    if (valsRead != sunBreaksCount)
        throw DatasetReadException("sunBreaksPol");

    int zenithBreaksCount = 0;
    valsRead              = fread(&zenithBreaksCount, sizeof(int), 1, handle);
    if (valsRead != 1 || zenithBreaksCount < 1)
        throw DatasetReadException("zenithBreaksCountPol");

    metadataPol.zenithBreaks.resize(zenithBreaksCount);
    valsRead = fread(metadataPol.zenithBreaks.data(), sizeof(double), zenithBreaksCount, handle);
    if (valsRead != zenithBreaksCount)
        throw DatasetReadException("zenithBreaksPol");

    metadataPol.emphBreaks.clear();

    // Calculate offsets and strides.

    metadataPol.sunOffset = 0;
    metadataPol.sunStride = metadataPol.sunBreaks.size() + metadataPol.zenithBreaks.size();

    metadataPol.zenithOffset = metadataPol.sunOffset + metadataPol.sunBreaks.size();
    metadataPol.zenithStride = metadataPol.sunStride;

    metadataPol.emphOffset = 0;

    metadataPol.totalCoefsSingleConfig =
        metadataPol.sunOffset +
        metadataPol.rank * metadataPol.sunStride; // this is for one specific configuration
    metadataPol.totalCoefsAllConfigs = metadataPol.totalCoefsSingleConfig * totalConfigs;

    // Read data.

    // Structure of the data part of the data file:
    // [[[[[[ sunCoefsPol       (sunBreaksCountPol * float), 
    //        zenithCoefsPol (zenithBreaksCountPol * float) ] * rankPol] 
    // * channels ] * elevationCount ] * altitudeCount ] * albedoCount ] * visibilityCount

    size_t offset = 0;
    dataPol.resize(metadataPol.totalCoefsAllConfigs);

    size_t oneConfigByteCount =
        (metadataPol.sunBreaks.size() + metadataPol.zenithBreaks.size()) * sizeof(float) * metadataPol.rank;

    // If a single visibility was requested, skip all configurations from the beginning till those needed for
    // the requested visibility.
    fseek(handle, oneConfigByteCount * skippedConfigsBegin, SEEK_CUR);

    for (int con = 0; con < totalConfigs; ++con) {
        for (int r = 0; r < metadataPol.rank; ++r) {
            // Read sun params.
            valsRead = fread(dataPol.data() + offset, sizeof(float), metadataPol.sunBreaks.size(), handle);
            if (valsRead != metadataPol.sunBreaks.size())
                throw DatasetReadException("sunCoefsPol");
            offset += metadataPol.sunBreaks.size();

            // Read zenith params.
            valsRead = fread(dataPol.data() + offset, sizeof(float), metadataPol.zenithBreaks.size(), handle);
            if (valsRead != metadataPol.zenithBreaks.size())
                throw DatasetReadException("zenithCoefsPol");
            offset += metadataPol.zenithBreaks.size();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////////////
// Initialization
/////////////////////////////////////////////////////////////////////////////////////

void PragueSkyModel::initialize(const std::string& filename, const double singleVisibility) {
    if (FILE* handle = fopen(filename.c_str(), "rb")) {
        initialized = false;
        // Read data
        readRadiance(handle, singleVisibility);
        readTransmittance(handle);
        readPolarisation(handle);
        fclose(handle);
        initialized = true;
    } else {
        throw DatasetNotFoundException(filename);
    }
}

PragueSkyModel::AvailableData PragueSkyModel::getAvailableData() const {
    if (!initialized) {
        throw NotInitializedException();
    }

    AvailableData available;
    available.albedoMin     = albedosRad.front();
    available.albedoMax     = albedosRad.back();
    available.altitudeMin   = altitudesRad.front();
    available.altitudeMax   = altitudesRad.back();
    available.elevationMin  = elevationsRad.front();
    available.elevationMax  = elevationsRad.back();
    available.visibilityMin = visibilitiesRad.front();
    available.visibilityMax = visibilitiesRad.back();
    available.polarisation  = (metadataPol.rank != 0);
    available.channels      = channels;
    available.channelStart  = channelStart;
    available.channelWidth  = channelWidth;

    return available;
}


/////////////////////////////////////////////////////////////////////////////////////
// Filling the Parameters structure
/////////////////////////////////////////////////////////////////////////////////////

PragueSkyModel::Parameters PragueSkyModel::computeParameters(const Vector3& viewpoint,
                                                             const Vector3& viewDirection,
                                                             const double   groundLevelSolarElevationAtOrigin,
                                                             const double   groundLevelSolarAzimuthAtOrigin,
                                                             const double   visibility,
                                                             const double   albedo) const {
    assert(viewpoint.z >= 0.0);
    assert(magnitude(viewDirection) > 0.0);
    assert(visibility >= 0.0);
    assert(albedo >= 0.0 && albedo <= 1.0);

    Parameters params;
    params.visibility = visibility;
    params.albedo     = albedo;

    // Shift viewpoint about safety altitude up

    const Vector3 centerOfTheEarth   = Vector3(0.0, 0.0, -PLANET_RADIUS);
    Vector3       toViewpoint        = viewpoint - centerOfTheEarth;
    Vector3       toViewpointN       = normalize(toViewpoint);
    const double  distanceToView     = magnitude(toViewpoint) + SAFETY_ALTITUDE;
    Vector3       toShiftedViewpoint = toViewpointN * distanceToView;
    Vector3       shiftedViewpoint   = centerOfTheEarth + toShiftedViewpoint;

    Vector3 viewDirectionN = normalize(viewDirection);

    // Compute altitude of viewpoint

    params.altitude = distanceToView - PLANET_RADIUS;
    params.altitude = std::max(params.altitude, 0.0);

    // Direction to sun

    Vector3 directionToSunN;
    directionToSunN.x = cos(groundLevelSolarAzimuthAtOrigin) * cos(groundLevelSolarElevationAtOrigin);
    directionToSunN.y = sin(groundLevelSolarAzimuthAtOrigin) * cos(groundLevelSolarElevationAtOrigin);
    directionToSunN.z = sin(groundLevelSolarElevationAtOrigin);

    // Solar elevation at viewpoint (more precisely, solar elevation at the point
    // on the ground directly below viewpoint)

    const double dotZenithSun = dot(toViewpointN, directionToSunN);
    params.elevation          = 0.5 * PI - acos(dotZenithSun);

    // Altitude-corrected view direction

    Vector3 correctViewN;
    if (distanceToView > PLANET_RADIUS) {
        Vector3 lookAt = shiftedViewpoint + viewDirectionN;

        const double correction =
            sqrt(distanceToView * distanceToView - PLANET_RADIUS * PLANET_RADIUS) / distanceToView;

        Vector3 toNewOrigin = toViewpointN * (distanceToView - correction);
        Vector3 newOrigin   = centerOfTheEarth + toNewOrigin;
        Vector3 correctView = lookAt - newOrigin;

        correctViewN = normalize(correctView);
    } else {
        correctViewN = viewDirectionN;
    }

    // Sun angle (gamma) - no correction

    double dotProductSun = dot(viewDirectionN, directionToSunN);
    params.gamma         = acos(dotProductSun);

    // Shadow angle - requires correction

    const double effectiveElevation = groundLevelSolarElevationAtOrigin;
    const double effectiveAzimuth   = groundLevelSolarAzimuthAtOrigin;
    const double shadowAngle        = effectiveElevation + PI * 0.5;

    Vector3 shadowDirectionN = Vector3(cos(shadowAngle) * cos(effectiveAzimuth),
                                       cos(shadowAngle) * sin(effectiveAzimuth),
                                       sin(shadowAngle));

    const double dotProductShadow = dot(correctViewN, shadowDirectionN);
    params.shadow                 = acos(dotProductShadow);

    // Zenith angle (theta) - corrected version stored in otherwise unused zero
    // angle

    double cosThetaCor = dot(correctViewN, toViewpointN);
    params.zero        = acos(cosThetaCor);

    // Zenith angle (theta) - uncorrected version goes outside

    double cosTheta = dot(viewDirectionN, toViewpointN);
    params.theta    = acos(cosTheta);

    return params;
}


/////////////////////////////////////////////////////////////////////////////////////
// Model evaluation for sky radiance and polarisation
/////////////////////////////////////////////////////////////////////////////////////

/// Evaluates piecewise linear approximation
double evalPL(const std::vector<float>::const_iterator coefs, const double factor) {
    return (double(coefs[1]) - double(coefs[0])) * factor + double(coefs[0]);
}

std::vector<float>::const_iterator PragueSkyModel::getCoefficients(const std::vector<float>& dataset,
                                                                   const int totalCoefsSingleConfig,
                                                                   const int elevation,
                                                                   const int altitude,
                                                                   const int visibility,
                                                                   const int albedo,
                                                                   const int wavelength) const {
    return dataset.cbegin() +
           (totalCoefsSingleConfig *
            (wavelength + size_t(channels) * elevation + channels * elevationsRad.size() * altitude +
             channels * elevationsRad.size() * altitudesRad.size() * albedo +
             channels * elevationsRad.size() * altitudesRad.size() * albedosRad.size() * visibility));
}

PragueSkyModel::InterpolationParameter PragueSkyModel::getInterpolationParameter(
    const double               queryVal,
    const std::vector<double>& breaks) const {
    // Clamp the value to the valid range.
    const double clamped = std::clamp(queryVal, breaks.front(), breaks.back());

    // Get the nearest greater parameter value.
    const auto next = std::upper_bound(breaks.cbegin() + 1, breaks.cend(), clamped);

    // Compute the index and float factor.
    InterpolationParameter parameter;
    parameter.index = int(next - breaks.cbegin()) - 1;
    if (next == breaks.cend()) {
        parameter.factor = 0.0;
    } else {
        parameter.factor = (clamped - *(next - 1)) / (*next - *(next - 1));
    }

    assert(0 <= parameter.index && parameter.index < breaks.size() &&
           (parameter.index < breaks.size() - 1 || parameter.factor == 0.f));
    assert(0 <= parameter.factor && parameter.factor <= 1);
    return parameter;
}

double PragueSkyModel::reconstruct(const AngleParameters&                   radianceParameters,
                                   const std::vector<float>::const_iterator channelParameters,
                                   const Metadata&                          metadata) const {
    // The original image was emphasized (for radiance only), re-parametrized into gamma-alpha space,
    // decomposed into sum of rankRad outer products of 'sun' and 'zenith' vectors and these vectors were
    // stored as piece-wise polynomial approximation. Here the process is reversed (for one point).

    double result = 0.0;
    for (int r = 0; r < metadata.rank; ++r) {
        // Restore the right value in the 'sun' vector
        const double sunParam = evalPL(channelParameters + metadata.sunOffset + r * metadata.sunStride +
                                           radianceParameters.gamma.index,
                                       radianceParameters.gamma.factor);

        // Restore the right value in the 'zenith' vector
        const double zenithParam = evalPL(channelParameters + metadata.zenithOffset +
                                              r * metadata.zenithStride + radianceParameters.alpha.index,
                                          radianceParameters.alpha.factor);

        // Accumulate their "outer" product
        result += sunParam * zenithParam;
    }

    // De-emphasize (for radiance only)
    if (!metadata.emphBreaks.empty()) {
        const double emphParam =
            evalPL(channelParameters + metadata.emphOffset + radianceParameters.zero.index,
                   radianceParameters.zero.factor);
        result *= emphParam;
        result = std::max(result, 0.0);
    }

    return result;
}

double PragueSkyModel::evaluateModel(const Parameters&         params,
                                     const double              wavelength,
                                     const std::vector<float>& data,
                                     const Metadata&           metadata) const {
    // Ignore wavelengths outside the dataset range.
    if (wavelength < channelStart || wavelength >= (channelStart + channels * channelWidth)) {
        return 0.0;
    }
    // Don't interpolate wavelengths inside the dataset range.
    const int channelIndex = int(floor((wavelength - channelStart) / channelWidth));

    // Translate angle values to indices and interpolation factors.
    AngleParameters angleParameters;
    angleParameters.gamma = getInterpolationParameter(params.gamma, metadata.sunBreaks);
    if (!metadata.emphBreaks.empty()) { // for radiance
        angleParameters.alpha =
            getInterpolationParameter(params.elevation < 0.0 ? params.shadow : params.zero,
                                      metadata.zenithBreaks);
        angleParameters.zero = getInterpolationParameter(params.zero, metadata.emphBreaks);
    } else { // for polarisation
        angleParameters.alpha = getInterpolationParameter(params.zero, metadata.zenithBreaks);
    }

    // Translate configuration values to indices and interpolation factors.
    const InterpolationParameter visibilityParam =
        getInterpolationParameter(params.visibility, visibilitiesRad);
    const InterpolationParameter albedoParam   = getInterpolationParameter(params.albedo, albedosRad);
    const InterpolationParameter altitudeParam = getInterpolationParameter(params.altitude, altitudesRad);
    const InterpolationParameter elevationParam =
        getInterpolationParameter(radiansToDegrees(params.elevation), elevationsRad);

    // Prepare parameters controlling the interpolation.
    ControlParameters controlParameters;
    for (int i = 0; i < 16; ++i) {
        const int visibilityIndex = std::min(visibilityParam.index + i / 8, int(visibilitiesRad.size() - 1));
        const int albedoIndex     = std::min(albedoParam.index + (i % 8) / 4, int(albedosRad.size() - 1));
        const int altitudeIndex   = std::min(altitudeParam.index + (i % 4) / 2, int(altitudesRad.size() - 1));
        const int elevationIndex  = std::min(elevationParam.index + i % 2, int(elevationsRad.size() - 1));

        controlParameters.coefficients[i] = getCoefficients(data,
                                                            metadata.totalCoefsSingleConfig,
                                                            elevationIndex,
                                                            altitudeIndex,
                                                            visibilityIndex,
                                                            albedoIndex,
                                                            channelIndex);
    }
    controlParameters.interpolationFactor[0] = visibilityParam.factor;
    controlParameters.interpolationFactor[1] = albedoParam.factor;
    controlParameters.interpolationFactor[2] = altitudeParam.factor;
    controlParameters.interpolationFactor[3] = elevationParam.factor;

    // Interpolate.
    const double result = interpolate<0, 0>(angleParameters, controlParameters, metadata);
    assert(metadata.emphBreaks.empty() || result >= 0.0); // polarisation can be negative

    return result;
}


/////////////////////////////////////////////////////////////////////////////////////
// Sky radiance
/////////////////////////////////////////////////////////////////////////////////////

double PragueSkyModel::skyRadiance(const Parameters& params, const double wavelength) const {
    if (!initialized) {
        throw NotInitializedException();
    }

	return evaluateModel(params, wavelength, dataRad, metadataRad);
}


/////////////////////////////////////////////////////////////////////////////////////
// Sun radiance
/////////////////////////////////////////////////////////////////////////////////////

double PragueSkyModel::sunRadiance(const Parameters& params, const double wavelength) const {
    if (!initialized) {
        throw NotInitializedException();
    }

    // Ignore wavelengths outside the dataset range.
    if (wavelength < SUN_RAD_START || wavelength >= SUN_RAD_END) {
        return 0.0;
    }

    // Return zero for rays not hitting the sun.
    if (params.gamma > SUN_RADIUS) {
        return 0.0;
    }

    // Compute index into the sun radiance table.
    const double idx = (wavelength - SUN_RAD_START) / SUN_RAD_STEP;
    assert(idx >= 0 && idx < std::size(SUN_RAD_TABLE) - 1);
    const int    idxInt   = int(floor(idx));
    const double idxFloat = idx - floor(idx);

    // Interpolate between the two closest values in the sun radiance table.
    const double sunRadiance =
        SUN_RAD_TABLE[idxInt] * (1.0 - idxFloat) + SUN_RAD_TABLE[idxInt + 1] * idxFloat;
    assert(sunRadiance > 0.0);

    // Compute transmittance towards the sun.
    const double tau = PragueSkyModel::transmittance(params, wavelength, std::numeric_limits<double>::max());
    assert(tau >= 0.0 && tau <= 1.0);

    // Combine.
    return sunRadiance * tau;
}


/////////////////////////////////////////////////////////////////////////////////////
// Polarisation
/////////////////////////////////////////////////////////////////////////////////////

double PragueSkyModel::polarisation(const Parameters& params, const double wavelength) const {
    if (!initialized) {
        throw NotInitializedException();
    }

    // If no polarisation data available
    if (metadataPol.rank == 0) {
        throw NoPolarisationException();
    }

    return -evaluateModel(params, wavelength, dataPol, metadataPol);
}


/////////////////////////////////////////////////////////////////////////////////////
// Transmittance
/////////////////////////////////////////////////////////////////////////////////////

std::vector<float>::const_iterator PragueSkyModel::getCoefficientsTrans(const int visibility,
                                                                        const int altitude,
                                                                        const int wavelength) const {
    return dataTransV.cbegin() +
           ((visibility * altitudesTrans.size() + altitude) * channels + wavelength) * rankTrans;
}

std::vector<float>::const_iterator PragueSkyModel::getCoefficientsTransBase(const int altitude,
                                                                            const int a,
                                                                            const int d) const {
    return dataTransU.cbegin() + size_t(altitude) * aDim * dDim * rankTrans +
           (size_t(d) * aDim + a) * rankTrans;
}

/// Intersects the given ray (assuming rayPosX == 0) with a circle at origin with the given radius.
/// \return In the case the ray intersects the circle, distance to the circle is returned, otherwise the
///         function returns negative number.
double intersectRayWithCircle2D(const double rayDirX,
                                const double rayDirY,
                                const double rayPosY,
                                const double circleRadius) {
    assert(rayPosY > 0.0);
    assert(circleRadius > 0.0);

    // Compute discriminant
    const double qA      = rayDirX * rayDirX + rayDirY * rayDirY;
    const double qB      = 2.0 * rayPosY * rayDirY;
    const double qC      = rayPosY * rayPosY - circleRadius * circleRadius;
    double       discrim = qB * qB - 4.0 * qA * qC;

    // No intersection or touch only
    if (discrim <= 0.0) {
        return -1.0;
    }

    discrim = std::sqrt(discrim);

    // Compute distances to both intersections
    const double d1 = (-qB + discrim) / (2.0 * qA);
    const double d2 = (-qB - discrim) / (2.0 * qA);

    // Try to take the nearest positive one
    const double distToIsect = (d1 > 0.0 && d2 > 0.0) ? std::min(d1, d2) : std::max(d1, d2);
    return distToIsect;
}

/// Auxiliary function for \ref toTransmittanceParams. Computes [altitude, distance] parameters from
/// coordinates of intersection of view ray and ground or atmosphere edge. Note: using floats here instead of
/// doubles will cause banding artifacts.
std::tuple<double, double> isectToAltitudeDistance(const double isectX, const double isectY) {
    // Distance to the intersection from world origin (not along ray as distToIsect in the calling method).
    const double isectDist = std::sqrt(isectX * isectX + isectY * isectY);
    assert(isectDist > 0.0);

    // Compute normalized and non-linearly scaled position in the atmosphere
    double altitude = std::clamp(isectDist - PLANET_RADIUS, 0.0, ATMOSPHERE_WIDTH);
    altitude        = std::pow(altitude / ATMOSPHERE_WIDTH, 1.0 / 3.0);
    double distance = std::acos(isectY / isectDist) * PLANET_RADIUS;
    distance        = std::sqrt(distance / DIST_TO_EDGE);
    distance        = std::sqrt(distance); // Calling twice sqrt, since it is faster than pow(...,0.25)
    distance        = std::min(1.0, distance);
    assert(0.0 <= altitude && altitude <= 1.0);
    assert(0.0 <= distance && distance <= 1.0);

    return std::make_tuple(altitude, distance);
}

PragueSkyModel::InterpolationParameter PragueSkyModel::getInterpolationParameterTrans(const double value,
                                                                                      const int    paramCount,
                                                                                      const int power) const {
    InterpolationParameter param;
    param.index  = std::min(int(value * paramCount), paramCount - 1);
    param.factor = 0.0;
    if (param.index < paramCount - 1) {
        param.factor =
            nonlerp(double(param.index) / paramCount, double(param.index + 1) / paramCount, value, power);
        param.factor = std::clamp(param.factor, 0.0, 1.0);
    }
    return param;
}

PragueSkyModel::TransmittanceParameters PragueSkyModel::toTransmittanceParams(const double theta,
                                                                              const double distance,
                                                                              const double altitude) const {
    assert(theta >= 0.0 && theta <= PI);
    assert(distance >= 0.0);
    assert(altitude >= 0.0);

    const double rayDirX = sin(theta);
    const double rayDirY = cos(theta);
    const double rayPosY = PLANET_RADIUS + altitude; // rayPosX == 0

    constexpr double ATMOSPHERE_EDGE = PLANET_RADIUS + ATMOSPHERE_WIDTH;

    // Find intersection of the ground-to-sun ray with edge of the atmosphere (in 2D)
    double           distToIsect  = -1.0;
    constexpr double LOW_ALTITUDE = 0.3;
    if (altitude < LOW_ALTITUDE) {
        // Special handling of almost zero altitude case to avoid numerical issues.
        if (theta <= 0.5 * PI) {
            distToIsect = intersectRayWithCircle2D(rayDirX, rayDirY, rayPosY, ATMOSPHERE_EDGE);
        } else {
            distToIsect = 0.0;
        }
    } else {
        distToIsect = intersectRayWithCircle2D(rayDirX, rayDirY, rayPosY, PLANET_RADIUS);
        if (distToIsect < 0.0) {
            distToIsect = intersectRayWithCircle2D(rayDirX, rayDirY, rayPosY, ATMOSPHERE_EDGE);
        }
    }
    // The ray should always hit either the edge of the atmosphere or the planet (we are starting inside the
    // atmosphere).
    assert(distToIsect >= 0.0);

    distToIsect = std::min(distToIsect, distance);

    // Compute intersection coordinates
    const double isectX = rayDirX * distToIsect;
    const double isectY = rayDirY * distToIsect + rayPosY;

    // Get the internal [altitude, distance] parameters
    const auto [altitudeParam, distanceParam] = isectToAltitudeDistance(isectX, isectY);

    // Convert to interpolation parameters
    TransmittanceParameters params;
    params.altitude = getInterpolationParameterTrans(altitudeParam, aDim, 3);
    params.distance = getInterpolationParameterTrans(distanceParam, dDim, 4);

    return params;
}

double PragueSkyModel::reconstructTrans(const int                     visibilityIndex,
                                        const int                     altitudeIndex,
                                        const TransmittanceParameters transParams,
                                        const int                     channelIndex) const {
    const std::vector<float>::const_iterator coefs =
        getCoefficientsTrans(visibilityIndex, altitudeIndex, channelIndex);

    // Load transmittance values for bi-linear interpolation
    std::array<double, 4> transmittance = { 0.0, 0.0, 0.0, 0.0 };
    int                   index         = 0;
    for (int a = transParams.altitude.index; a <= transParams.altitude.index + 1; ++a) {
        if (a < aDim) {
            for (int d = transParams.distance.index; d <= transParams.distance.index + 1; ++d) {
                if (d < dDim) {
                    const std::vector<float>::const_iterator baseCoefs =
                        getCoefficientsTransBase(altitudeIndex, a, d);
                    for (int i = 0; i < rankTrans; ++i) {
                        // Reconstruct transmittance value
                        transmittance[index] += double(baseCoefs[i]) * double(coefs[i]);
                    }
                    index++;
                }
            }
        }
    }

    // Perform bi-linear interpolation
    if (transParams.distance.factor > 0.f) {
        transmittance[0] = lerpPrague(transmittance[0], transmittance[1], transParams.distance.factor);
        transmittance[1] = lerpPrague(transmittance[2], transmittance[3], transParams.distance.factor);
    }
    transmittance[0] = std::max(transmittance[0], 0.0);

    if (transParams.altitude.factor > 0.f) {
        transmittance[1] = std::max(transmittance[1], 0.0);
        transmittance[0] = lerpPrague(transmittance[0], transmittance[1], transParams.altitude.factor);
    }

    assert(transmittance[0] >= 0.0);
    return transmittance[0];
}

double PragueSkyModel::interpolateTrans(const int                     visibilityIndex,
                                        const InterpolationParameter  altitudeParam,
                                        const TransmittanceParameters transParams,
                                        const int                     channelIndex) const {
    // Get transmittance for the nearest lower altitude.
    double trans = reconstructTrans(visibilityIndex, altitudeParam.index, transParams, channelIndex);

    // Interpolate with transmittance for the nearest higher altitude if needed.
    if (altitudeParam.factor > 0.0) {
        const double transHigh =
            reconstructTrans(visibilityIndex, altitudeParam.index + 1, transParams, channelIndex);
        trans = lerpPrague(trans, transHigh, altitudeParam.factor);
    }

    return trans;
}

double PragueSkyModel::transmittance(const Parameters& params,
                                     const double      wavelength,
                                     const double      distance) const {
    assert(distance > 0.0);

    if (!initialized) {
        throw NotInitializedException();
    }

    // Ignore wavelengths outside the dataset range.
    if (wavelength < channelStart || wavelength >= (channelStart + channels * channelWidth)) {
        return 0.0;
    }
    // Don't interpolate wavelengths inside the dataset range.
    const int channelIndex = int(floor((wavelength - channelStart) / channelWidth));

    // Translate configuration values to indices and interpolation factors.
    const InterpolationParameter visibilityParam =
        getInterpolationParameter(params.visibility, visibilitiesTrans);
    const InterpolationParameter altitudeParam = getInterpolationParameter(params.altitude, altitudesTrans);

    // Calculate position in the atmosphere.
    const TransmittanceParameters transParams = toTransmittanceParams(params.theta, distance, params.altitude);

    // Get transmittance for the nearest lower visibility.
    double trans = interpolateTrans(visibilityParam.index, altitudeParam, transParams, channelIndex);

    // Interpolate with transmittance for the nearest higher visibility if needed.
    if (visibilityParam.factor > 0.0) {
        const double transHigh =
            interpolateTrans(visibilityParam.index + 1, altitudeParam, transParams, channelIndex);

        trans = lerpPrague(trans, transHigh, visibilityParam.factor);
    }

    // Transmittance is stored as a square root. Needs to be restored here.
    trans = trans * trans;
    assert(trans >= 0.0 && trans <= 1.0);

    return trans;
}