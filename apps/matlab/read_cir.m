tcp = tcpclient('127.0.0.1', 19021);
pause(1)
      
ntime =3000
idx =[];
data=[];
fp_idx =[];
cir =[];
cir_current =[];
fp_idx_current =[];
t =[];


for j=1:ntime
   
    data = [data, read(tcp)];
    str = char(data);
    idx = find(str == 13);
    
    while (length(idx) < 4)
        data = [data, read(tcp)];
        str = char(data);
        idx = find(str == 13);
    end    
            
    for i=1:length(idx)-2
        line = str((idx(i)+2):idx(i+1));
        if (line(1) == '{')
            line = jsondecode(line);
            if (isstruct(line)) 
                if (isfield(line,'cir'))
                    cir(end+1,:) = complex(line.cir.real,line.cir.imag)';
                    fp_idx(end+1) = line.cir.fp_idx/pow2(2,5);
                end
            end
        end
        data = data(idx(i+1):end);
    end
    [n,m] = size(cir);
    
     if(n>1)
        t = (1:m) + 600;
        abs_cir_current = abs(cir(end,:));
        real_cir_current = real(cir(end,:));
        imag_cir_current = imag(cir(end,:));

        fp_idx_current = fp_idx(end,:);
     end
     
     if (j==1)
        linkdata off
        
        subplot(211);
        plot(t,abs_cir_current,'m-o');
        xlabel('utime')
        ylabel('range(m)')
        
        subplot(212);
        plot(t,[real_cir_current;imag_cir_current]);
        xlabel('utime')
        ylabel('range(m)')
        
        linkdata on
        shg
     
     else
        if (mod(m,32) == 0)
            pause(0.01)
            refreshdata;
        end
     end

%    for i=1:16:n
%        subplot(211);plot(t, [abs(cir(end,:))]); 
%        hold on; plot(fp_idx(end), 1000, '*'); hold off
%        drawnow;
%        subplot(212);plot(t, [real(cir(end,:)); imag(cir(end,:))]); 
%    end
end





