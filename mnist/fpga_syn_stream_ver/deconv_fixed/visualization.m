%%
clear; clc;

%%
filePath = 'C:\Users\Jack\Desktop\DCNN-on-FPGA\mnist\fpga_syn\fpga_syn\solution1\csim\build\out.csv';
expected = csvread(filePath);

figure(1);
imgs = reshape(expected, 28, 28, 25);
for i=1:25
    subplot(5, 5, i);
    imshow(imgs(:,:,i));
end


%%
filePath = 'C:\Users\Jack\Desktop\DCNN-on-FPGA\mnist\fpga_syn_stream_ver\deconv_fixed\deconv_fixed\solution1\csim\build\out.csv';
actual = csvread(filePath);
actual = reshape(actual, 1, size(actual,1)*size(actual,2));


figure(2)
imgs = reshape(expected, 28, 28, 25);
for i=1:25
    subplot(5, 5, i);
    imshow(imgs(:,:,i));
end





%%


