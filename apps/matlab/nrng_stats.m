tcp = tcpclient('127.0.0.1', 19021);

ntimes = 12000
idx = [];
data= [];
utime =[];
tdoa =[];
fp_idx =[];
rng = [];
mask = [];
skew = [];
skew_wcs = [];
utime_wcs = [];
wcs = [];

dtu = 1/(1e-6/2^16);
utime_timescale = [];
timescale = [];

tof_to_meter =@(x) (x * (299792458.0/1.000293) * (1.0/(499.2e6*128))/2);

for j=1:ntimes
    
    data = [data, read(tcp)];
    str = char(data);
    idx = find(str == 13);
    
    while (length(idx) < 16)
        data = [data, read(tcp)];
        str = char(data);
        idx = find(str == 13);
    end  
    

     for i=1:(length(idx) - 2)
        line = str((idx(i)+2):idx(i+1));
        if (line(1) == '{')
            line = jsondecode(line);
            if (isstruct(line)) 
                if (isfield(line,'utime') && isfield(line,'nrng'))
                    nrng = line.nrng;
                    if(isfield(nrng,'tdoa') && isfield(nrng,'mask') && isfield(nrng,'rng'))
                        utime(end+1) = line.utime/1e6;
                        mask(end+1) = nrng.mask;
                        tdoa_ = NaN(1,16);
                        for k = 1:16
                            idx_ = SlotIndex_mex(uint32(nrng.mask),uint32(bitshift(1,k-1)));
                            if ~isnan(idx_)
                                 tdoa_(k) = tof_to_meter(nrng.tdoa(idx_)); 
                            else
                                 tdoa_(k) = NaN;
                            end
                        end

                        tdoa(end+1,:) = tdoa_;
                        rng_ = NaN(1,16);
                        for k = 1:16
                            idx_ = SlotIndex_mex(uint32(nrng.mask),uint32(bitshift(1,k-1)));
                            if ~isnan(idx_)
                                 rng_(k) = typecast(uint32(nrng.rng(idx_)),'single'); 
                            else
                                 rng_(k) = NaN;
                            end
                        end
                        rng(end+1,:) = rng_;                       
                    end
                end
                if (isfield(line,'utime') && isfield(line,'wcs'))
                    utime_wcs(end+1) = line.utime/1e6;
                    wcs(end+1,:) = line.wcs;
                    skew(end+1) = typecast(uint64(line.skew),'double') * 1e6;
                end
                if (isfield(line,'utime') && isfield(line,'timescale'))
                    utime_timescale(end+1) = line.utime/1e6;
                    timescale(end+1,:) = typecast(uint64(line.timescale),'double');
                end
            end
        end
        data = data(idx(i+1):end);
     end
     nwin = 1000;
     [~,m] = size(utime);
     
     if (mod(j,1) == 0)
        pause(0.001)
        refreshdata;
     end
     if (mod(j,1) == 0)
            ax = subplot(311);
            plot(utime,tdoa);
            title(sprintf("tdoa (tdu)"))

            ax = subplot(312);
            plot(utime,rng);
            [mu,sigma,~,~] = normfit(rng);
            title(sprintf("rng (m)"))

            %  [mu,sigma,~,~] = normfit(rng);
          %  title(sprintf("rng (m)"))
      end
%      if (mod(j,4) == 0 && length(rng) > nwin)
%             [mu,sigma,~,~] = normfit(rng((end-nwin):end));
%             subplot(234);histfit(rng((end-nwin):end,:),16,'normal');title(sprintf("range mu=%f (m) sigma=%f (m)",mu,sigma))
%      end
%      if (mod(j,4) == 0 && length(skew) > nwin)
%             [mu,sigma,~,~] = normfit(skew((end-nwin):end));
%             subplot(235);histfit(skew((end-nwin):end),16,'normal');title(sprintf("skew mu=%f (usec) sigma=%f (usec)",mu,sigma))
%      end
%      if (mod(j,32) == 0 )
%             [mu,sigma,~,~] = normfit(skew_wcs);
%             subplot(236);histfit(skew_wcs,16,'normal');title(sprintf("wcs mu=%f (usec) sigma=%f (usec)",mu,sigma))
%      end

end
 


