tcp = tcpclient('127.0.0.1', 19021);

ntimes = 12000
idx = [];
data= [];
utime =[];
tof =[];
fp_idx =[];
range = [];
skew = [];
wcs = [];

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
                if (isfield(line,'utime') && isfield(line,'wcs'))
                    utime(end+1) = line.utime/1e6;
                    wcs(end+1,:) = line.wcs;
                    skew(end+1) = typecast(uint64(line.skew),'double') * 1e6;
                end
            end
        end
        data = data(idx(i+1):end);
     end
     nwin = 16;
     [~,m] = size(utime);
     
     if (mod(j,4) == 0)
        pause(0.04)
        refreshdata;
     end
     if (mod(j,4) == 0 && ~isempty(skew))
            ax = subplot(211);
            yyaxis(ax,'right')
            plot(utime,skew,'o-m');
            yyaxis(ax,'left')
            plot(utime,wcs(:,4))
     end
     if (mod(j,4) == 0 && length(skew) > nwin)
            [mu,sigma,~,~] = normfit(skew((end-nwin):end));
            subplot(235);histfit(skew((end-nwin):end),16,'normal');title(sprintf("skew mu=%f (usec) sigma=%f (usec)",mu,sigma))
     end

end
 


