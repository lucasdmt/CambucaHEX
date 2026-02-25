#include <cstring>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <cassert>
#include <pthread.h>
#include <fstream>
#include "GPU/GPUSecp.h"
#include "CPU/SECP256k1.h"
#include "CPU/HashMerge.cpp"
#include <sys/resource.h>
#include <chrono>

#include <cmath> //pow
#include <getopt.h>

#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"


bool lerChaveDeArquivo(const char* nomeArquivo, char* strCPU){
   std::ifstream inputFile(nomeArquivo);
   if (!inputFile){
      std::cerr << "Error opening file " << nomeArquivo << std::endl;
      return false;
   }

   std::string line;
   if (!std::getline(inputFile, line)){
      std::cerr << "Error reading line from file " << nomeArquivo << std::endl;
      return false;
   }

   if (line.length() != 64){
      std::cerr << "Error: the file line must contain exactly 64 characters." << std::endl;
      return false;
   }

   strncpy(strCPU, line.c_str(), 64);
   strCPU[64] = '\0';
   std::cout << "Key loaded from file" << std::endl;
   return true;
}

bool validarChaveHex(const char* strCPU, bool permitirX){
   for (int i = 0; i < 64; i++){
      char c = strCPU[i];

      bool hex =
         (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');

      bool x =
         (c == 'x' || c == 'X');

      if (!(hex || (permitirX && x))){
         std::cerr << "Erro: caractere inválido na posição "
                   << i << ": '" << c << "'" << std::endl;
         return false;
      }
   }

   return true;
}

bool hexStringToLittleEndian(const std::string& strCPU, uint8_t* privKeyCPU){
    // converte a string hex para uint8_t[32] em little-endian
    for (int i = 0; i < 32; i++) {
        // Lê os caracteres hex *do fim para o início* (inverte a ordem dos bytes)
        int posChar = (31 - i) * 2;  // Começa do último par de caracteres
        char highChar = strCPU[posChar];
        char lowChar  = strCPU[posChar + 1];

        // converte para nibbles e combina em um byte
        uint8_t highNibble =
            (highChar >= '0' && highChar <= '9') ? highChar - '0' :
            (highChar >= 'a' && highChar <= 'f') ? highChar - 'a' + 10 :
            (highChar >= 'A' && highChar <= 'F') ? highChar - 'A' + 10 :
            0; // qualquer coisa fora 0-9, a-f, A-F (ex: 'x') vira 0

        uint8_t lowNibble =
            (lowChar >= '0' && lowChar <= '9') ? lowChar - '0' :
            (lowChar >= 'a' && lowChar <= 'f') ? lowChar - 'a' + 10 :
            (lowChar >= 'A' && lowChar <= 'F') ? lowChar - 'A' + 10 :
            0;

        privKeyCPU[i] = (highNibble << 4) | lowNibble;
    }

    return true;
}


void loadInputHash(uint64_t *inputHashBufferCPU) {
   std::cout << "Loading hash buffer from file: " << NAME_HASH_BUFFER << std::endl;

   FILE *fileSortedHash = fopen(NAME_HASH_BUFFER, "rb");
   if (fileSortedHash == NULL)
   {
      printf("Error: not able to open input file: %s\n", NAME_HASH_BUFFER);
      exit(1);
   }

   fseek(fileSortedHash, 0, SEEK_END);
   long hashBufferSizeBytes = ftell(fileSortedHash);
   long hashCount = hashBufferSizeBytes / SIZE_LONG;
   rewind(fileSortedHash);

   if (hashCount != COUNT_INPUT_HASH) {
      printf("ERROR - Constant COUNT_INPUT_HASH is %d, but the actual hashCount is %lu \n", COUNT_INPUT_HASH, hashCount);
      exit(-1);
   }

   size_t size = fread(inputHashBufferCPU, 1, hashBufferSizeBytes, fileSortedHash);
   fclose(fileSortedHash);

   std::cout << "loadInputHash " << NAME_HASH_BUFFER << " finished!" << std::endl;
   std::cout << "hashCount: " << hashCount << ", hashBufferSizeBytes: " << hashBufferSizeBytes << std::endl;
}

void loadGTable(uint8_t *gTableX, uint8_t *gTableY) {
   std::cout << "loadGTable started" << std::endl;

   Secp256K1 *secp = new Secp256K1();
   secp->Init();

   for (int i = 0; i < NUM_GTABLE_CHUNK; i++)
   {
      for (int j = 0; j < NUM_GTABLE_VALUE - 1; j++)
      {
         int element = (i * NUM_GTABLE_VALUE) + j;
         Point p = secp->GTable[element];
         for (int b = 0; b < 32; b++) {
            gTableX[(element * SIZE_GTABLE_POINT) + b] = p.x.GetByte64(b);
            gTableY[(element * SIZE_GTABLE_POINT) + b] = p.y.GetByte64(b);
         }
      }
   }

   std::cout << "loadGTable finished!" << std::endl;
}

void salvarPosicoes(const char *strCPU, int *posicoesCPU, int *totalPosicoesCPU) {
    *totalPosicoesCPU = 0;
    for (int i = 0; strCPU[i] != '\0'; i++) {
        if (strCPU[i] == 'x') {
            posicoesCPU[*totalPosicoesCPU] = i;
            (*totalPosicoesCPU)++;
        }
    }
    printf("Total de posições 'x' encontradas:%d\n", *totalPosicoesCPU);
    printf("X nas posições no array:");
    for (int i = 0; i < *totalPosicoesCPU; i++) {
        printf("%d ", posicoesCPU[i]); 
    }
    printf("      ");
    printf("X nas posições na chave:");
    for (int i = 0; i < *totalPosicoesCPU; i++) {
        printf("%d ", posicoesCPU[i]+1); 
    }
    printf("\n");
}

void printProgressBar(double progress)
{
    const int barWidth = 30;

    printf("[");
    int pos = (int)(barWidth * progress);

    for (int i = 0; i < barWidth; i++)
    {
        if (i < pos) printf("█");
        else printf("░");
    }
    printf("]");
}
void printStrCPU(const char *strCPU, const int *posicoesCPU, int totalPosicoesCPUtemp){
   int mapa[64] = {0};

   // marca as posições que devem ficar vermelhas
   for (int j = 0; j < totalPosicoesCPUtemp; j++)
   {
      if (posicoesCPU[j] >= 0 && posicoesCPU[j] < 64)
         mapa[posicoesCPU[j]] = 1;
   }

   // imprime a chave
   printf(" ");
   for (int i = 0; i < 64; i++)
   {
      if (mapa[i])
         printf(COLOR_RED "%c" COLOR_RESET, strCPU[i]);
      else
         printf("%c", strCPU[i]);
   }
}

void startSecp256k1ModeHexX(uint8_t * gTableXCPU, uint8_t * gTableYCPU, uint64_t * inputHashBufferCPU, const char* strCPU) {

   printf("Modo Hex X\n");
   uint8_t privKeyCPU[SIZE_PRIV_KEY];

   hexStringToLittleEndian(strCPU, privKeyCPU);

   /*printf("privkey cpu hex: ");
   for (int i = 0; i < 32; ++i) {
      printf("%02X", (unsigned)privKeyCPU[i]);
   }printf("\n");*/

   int posicoesCPU[65];
   int totalPosicoesCPUtemp;

   salvarPosicoes(strCPU, posicoesCPU, &totalPosicoesCPUtemp);

   GPUSecp *gpuSecp = new GPUSecp(
      gTableXCPU,
      gTableYCPU,
      inputHashBufferCPU,
      privKeyCPU,              
      posicoesCPU,         
      totalPosicoesCPUtemp
   );

   long timeTotal = 0;
   long totalCount = (COUNT_CUDA_THREADS);

    long long possibilidades = 1;
    for(int i = 0; i < totalPosicoesCPUtemp; i++)possibilidades *= 16;

   printf("Chave Parcial char: ");
   printStrCPU(strCPU, posicoesCPU, totalPosicoesCPUtemp);
   printf("\nPossibilidades por combinação (16^%d): %llu\n", totalPosicoesCPUtemp, possibilidades);


   int itercount = COUNT_CUDA_THREADS * THREAD_MULT;
   long long maxIteration = (possibilidades + itercount - 1) / itercount;
   printf("cada iteração resulta em %d tentativas, resultando em no maximo de %lld iterações \n", itercount, maxIteration);

   auto clockStart = std::chrono::high_resolution_clock::now();
   for (int iter = 0; iter < maxIteration + 1; iter++){

      const auto clockIter1 = std::chrono::high_resolution_clock::now();

      gpuSecp->doIterationSecp256k1Books(iter);
      gpuSecp->doPrintOutput();

      const auto clockIter2 = std::chrono::high_resolution_clock::now();

      long iterationDuration =std::chrono::duration_cast<std::chrono::milliseconds>(clockIter2 - clockIter1).count();
      timeTotal += iterationDuration;

      long currentGlobalIteration = iter + 1;
      double progress = (double)currentGlobalIteration / (maxIteration + 1);
      double avgMillisPerIter = (double)timeTotal / currentGlobalIteration;
      long remainingIterations = (maxIteration + 1) - currentGlobalIteration;
      long etaMillis = (long)(avgMillisPerIter * remainingIterations);

      int etaSeconds = etaMillis / 1000;
      int etaMinutes = etaSeconds / 60;
      int etaRemSeconds = etaSeconds % 60;

      // ===== SPEED =====
      double totalKeysTested =(double)currentGlobalIteration * itercount;
      double speedKeysPerSec =totalKeysTested / (timeTotal / 1000.0);
      double speedMKeys = speedKeysPerSec / 1000000.0;

      printf("\r");
      printProgressBar(progress);
      printf(" %6.2f%% |%8.2f Mkeys/s | ETA: %02dm %02ds | iteration %d/%lld", progress * 100.0, speedMKeys, etaMinutes, etaRemSeconds, iter, maxIteration);
      fflush(stdout);
   }
   auto clockEnd = std::chrono::high_resolution_clock::now();
   long long timeTotal2 =std::chrono::duration_cast<std::chrono::milliseconds>(clockEnd - clockStart).count();

   long long totalKeysTested =(long long)(maxIteration) * itercount;
   double totalSeconds = timeTotal2 / 1000.0;
   double avgKeysPerSec =totalKeysTested / totalSeconds;
   double avgMKeysPerSec =avgKeysPerSec / 1000000.0;

   printf("Tempo total: %.2f segundos\n", totalSeconds);
   printf("Total de chaves testadas: %lld\n", totalKeysTested);
   printf("Velocidade média: %.2f Mkeys/s\n", avgMKeysPerSec);
}


void startSecp256k1ModeHexScanL(uint8_t * gTableXCPU, uint8_t * gTableYCPU, uint64_t * inputHashBufferCPU, int positions, const char* strCPU)
{
   printf("Mode Linear HEX scan\n");    
   printf("Chave Parcial char: %s\n", strCPU);

   uint8_t privKeyCPU[SIZE_PRIV_KEY];

   hexStringToLittleEndian(strCPU, privKeyCPU);

   int posicoesCPU[65] = {0};
   int totalPosicoesCPUtemp;


   GPUSecp *gpuSecp = new GPUSecp(
      gTableXCPU,
      gTableYCPU,
      inputHashBufferCPU,
      privKeyCPU,
      posicoesCPU,
      totalPosicoesCPUtemp
   );

   long timeTotal = 0;

   totalPosicoesCPUtemp = positions;
   for(int x=0;x<totalPosicoesCPUtemp;x++){
      posicoesCPU[x]=x;
   };

   long long possibilidades = pow(16, totalPosicoesCPUtemp) - 1;

   int itercount = COUNT_CUDA_THREADS * THREAD_MULT;
   long long maxIteration = (possibilidades + itercount - 1) / itercount;

   int maxscan = 64 - totalPosicoesCPUtemp;

   long totalGlobalIterations = (long)maxscan * (maxIteration + 1);

   printf("Total iterações globais: %ld\n", totalGlobalIterations);
   auto clockStart = std::chrono::high_resolution_clock::now();

   for(int ms = 0; ms < maxscan; ms++){
      for(int r = 0; r < totalPosicoesCPUtemp; r++) posicoesCPU[r]++;

      for(int iter = 0; iter < maxIteration + 1; iter++){
         const auto clockIter1 = std::chrono::high_resolution_clock::now();

         gpuSecp->updatemodohex(posicoesCPU, totalPosicoesCPUtemp);
         gpuSecp->doIterationSecp256k1Books(iter);
         gpuSecp->doPrintOutput();

         const auto clockIter2 = std::chrono::high_resolution_clock::now();
         long iterationDuration = std::chrono::duration_cast<std::chrono::milliseconds>(clockIter2 - clockIter1).count();

         timeTotal += iterationDuration;

            // ===== CÁLCULO GLOBAL =====

         long currentGlobalIteration = (long)ms * (maxIteration + 1) + iter + 1;
         double progress = (double)currentGlobalIteration / totalGlobalIterations;
         long elapsedMillis = timeTotal;

         double avgMillisPerIter = (double)elapsedMillis / currentGlobalIteration;
         long remainingIterations = totalGlobalIterations - currentGlobalIteration;
         long etaMillis = (long)(avgMillisPerIter * remainingIterations);
         int etaSeconds = etaMillis / 1000;
         int etaMinutes = etaSeconds / 60;
         int etaRemSeconds = etaSeconds % 60;

            // ===== SPEED =====

         double totalKeysTested = (double)currentGlobalIteration * itercount;
         double speedKeysPerSec = totalKeysTested / (elapsedMillis / 1000.0);
         double speedMKeys = speedKeysPerSec / 1000000.0;

         printf("\r");
         printProgressBar(progress); 
         printf(" %6.2f%% | %8.2f Mkeys/s | ETA: %02dm %02ds | Scan %d/%d", progress * 100.0, speedMKeys, etaMinutes, etaRemSeconds, ms + 1, maxscan);
         printf("\n");
         printStrCPU(strCPU, posicoesCPU, totalPosicoesCPUtemp);
         printf("\033[1A");
         fflush(stdout);
      }
   }
   auto clockEnd = std::chrono::high_resolution_clock::now();
   long long timeTotal2 =std::chrono::duration_cast<std::chrono::milliseconds>(clockEnd - clockStart).count();

   long long totalKeysTested =(long long)(totalGlobalIterations*itercount);
   double totalSeconds = timeTotal2 / 1000.0;
   double avgKeysPerSec =totalKeysTested / totalSeconds;
   double avgMKeysPerSec =avgKeysPerSec / 1000000.0;

   printf("\nTempo total: %.2f segundos\n", totalSeconds);
   printf("Total de chaves testadas: %lld\n", totalKeysTested);
   printf("Velocidade média: %.2f Mkeys/s\n", avgMKeysPerSec);
}

unsigned long long calcularCombinacoes64(int k){
   if (k < 0 || k > 64) return 0;
   if (k == 0 || k == 64) return 1;

   if (k > 32)
      k = 64 - k; // otimização

   unsigned long long resultado = 1;

   for (int i = 0; i < k; i++){
      resultado = resultado * (64 - i);
      resultado = resultado / (i + 1);
   }
   return resultado;
}
int proximaCombinacao(int *posicoesCPU, int k, int max){
   for(int i = k - 1; i >= 0; i--){
      if(posicoesCPU[i] < max - k + i){
         posicoesCPU[i]++;

         for(int j = i + 1; j < k; j++)
            posicoesCPU[j] = posicoesCPU[j - 1] + 1;
            return 1;
      }
   }
   return 0;
}

void startSecp256k1ModeHexComb(uint8_t * gTableXCPU, uint8_t * gTableYCPU, uint64_t * inputHashBufferCPU, int positions, const char* strCPU){
   printf("Mode Combination HEX scan\n");

   uint8_t privKeyCPU[SIZE_PRIV_KEY];
   hexStringToLittleEndian(strCPU, privKeyCPU);
       
   /*printf("privkey cpu hex: ");
   for (int i = 0; i < 32; ++i)
      printf("%02X", (unsigned)privKeyCPU[i]);
   printf("\n");*/

   int posicoesCPU[65] = {0};
   int totalPosicoesCPUtemp;

   GPUSecp *gpuSecp = new GPUSecp(
      gTableXCPU,
      gTableYCPU,
      inputHashBufferCPU,
      privKeyCPU,
      posicoesCPU,
      totalPosicoesCPUtemp
   );

   long timeTotal = 0;
   //inicializa o array posicoes
   totalPosicoesCPUtemp = positions; 
   for(int x=0;x<totalPosicoesCPUtemp;x++){
      posicoesCPU[x]=x;
   };

   long long possibilidades = 1;
   for(int i = 0; i < totalPosicoesCPUtemp; i++)possibilidades *= 16;

   printf("Chave Parcial char: %s\n", strCPU);
   printf("Possibilidades por combinação (16^%d): %llu\n", totalPosicoesCPUtemp, possibilidades);

   int itercount = COUNT_CUDA_THREADS * THREAD_MULT;
   long long maxIteration = (possibilidades + itercount - 1) / itercount;

    

   unsigned long long maxcombination = calcularCombinacoes64(totalPosicoesCPUtemp);
   long totalGlobalIterations = (long)maxcombination * (maxIteration + 1);

   printf("Numedo de combinações %lld Total iterações globais: %ld\n",maxcombination ,totalGlobalIterations);
   printf("cada iteração calcula %d chaves resultando no maximo de %lld iterações em cada combinação.",itercount ,maxIteration);

   auto clockStart = std::chrono::high_resolution_clock::now();
   for (int ms = 0; ms < maxcombination; ms++){
         for (int iter = 0; iter < maxIteration + 1; iter++){
            const auto clockIter1 = std::chrono::high_resolution_clock::now();

            gpuSecp->updatemodohex(posicoesCPU, totalPosicoesCPUtemp);
            gpuSecp->doIterationSecp256k1Books(iter);
            gpuSecp->doPrintOutput();

            const auto clockIter2 = std::chrono::high_resolution_clock::now();
            long iterationDuration =std::chrono::duration_cast<std::chrono::milliseconds>(clockIter2 - clockIter1).count();
            timeTotal += iterationDuration;

            // ===== CÁLCULO GLOBAL =====
            long currentGlobalIteration =(long)ms * (maxIteration + 1) + iter + 1;
            double progress = (double)currentGlobalIteration / totalGlobalIterations;
            long elapsedMillis = timeTotal;
            double avgMillisPerIter = (double)elapsedMillis / currentGlobalIteration;
            long remainingIterations = totalGlobalIterations - currentGlobalIteration;
            long etaMillis = (long)(avgMillisPerIter * remainingIterations);
            int etaSeconds = etaMillis / 1000;
            int etaMinutes = etaSeconds / 60;
            int etaRemSeconds = etaSeconds % 60;

            // ===== SPEED =====
            double totalKeysTested = (double)currentGlobalIteration * itercount;
            double speedKeysPerSec = totalKeysTested / (elapsedMillis / 1000.0);
            double speedMKeys = speedKeysPerSec / 1000000.0;
            printf("\r");
            printProgressBar(progress);
            printf(" %6.2f%% | %8.2f Mkeys/s | ETA: %02dm %02ds | comb %d/%lld", progress * 100.0, speedMKeys, etaMinutes, etaRemSeconds, ms + 1, maxcombination);
            printf("\n");
            printStrCPU(strCPU, posicoesCPU, totalPosicoesCPUtemp);
            printf("\033[1A");
            fflush(stdout);
      }
      proximaCombinacao(posicoesCPU,totalPosicoesCPUtemp, 64);
   }
   auto clockEnd = std::chrono::high_resolution_clock::now();
   long long timeTotal2 =std::chrono::duration_cast<std::chrono::milliseconds>(clockEnd - clockStart).count();

   long long totalKeysTested =(long long)(totalGlobalIterations*itercount);
   double totalSeconds = timeTotal2 / 1000.0;
   double avgKeysPerSec =totalKeysTested / totalSeconds;
   double avgMKeysPerSec =avgKeysPerSec / 1000000.0;

   printf("\nTempo total: %.2f segundos\n", totalSeconds);
   printf("Total de chaves testadas: %lld\n", totalKeysTested);
   printf("Velocidade média: %.2f Mkeys/s\n", avgMKeysPerSec);
}



void increaseStackSizeCPU() {
   const rlim_t cpuStackSize = SIZE_CPU_STACK;
   struct rlimit rl;
   int result;

   printf("Increasing Stack Size to %lu \n", cpuStackSize);

   result = getrlimit(RLIMIT_STACK, &rl);
   if (result == 0)
   {
      if (rl.rlim_cur < cpuStackSize)
      {
         rl.rlim_cur = cpuStackSize;
         result = setrlimit(RLIMIT_STACK, &rl);
         if (result != 0)
         {
            fprintf(stderr, "setrlimit returned result = %d\n", result);
         }
      }
   }
}

int main(int argc, char **argv) {
   printf("iniciando a cambuca \n");

    bool modeHexX   = false;
    bool modeLinear = false;
    bool modeComb   = false;
    int positions = -1;   // numero de posicoes para modo linear e comb
    bool key = false;
    char strCPUload[65] = {0};// 64 \0
    bool showHelp = false;

    const struct option long_options[] = {
        {"linear",      no_argument,       nullptr, 'l'},
        {"combination", no_argument,       nullptr, 'c'},
        {"hexx",        no_argument,       nullptr, 'x'},
        {"positions",   required_argument, nullptr, 'p'},
        {"key",         required_argument, nullptr, 'k'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}   // <--- precisa disso so nao sei o pq 
    };

    int opt;

     while ((opt = getopt_long(argc, argv, "lcxp:k:h", long_options, nullptr)) != -1){ //:significa que recebe argumentos seu bobao
        switch (opt)
        {
            case 'l':
                modeLinear = true;
                break;

            case 'c':
                modeComb = true;
                break;

            case 'x':
                modeHexX = true;
                break;

            case 'p':
                positions = atoi(optarg);
                if (positions <= 0 || positions > 8)
                {
                    fprintf(stderr, "Erro: --calma ai meu chapa\n");
                    return 1;
                }
                break;

            case 'h':
                showHelp = true;
                break;

            case 'k':
                if (strlen(optarg) != 64) {
                    fprintf(stderr, "Erro: chave deve ter 64 caracteres\n");
                    return 1;
                }
                strncpy(strCPUload, optarg, 64);
                strCPUload[64] = '\0';
                key = true;
                break;

            default:
                showHelp = true;
                break;
        }
    }

    int modeCount = 0;
    if (modeLinear) modeCount++;
    if (modeComb)   modeCount++;
    if (modeHexX)   modeCount++;

    if (modeCount == 0)
    {
        showHelp=true;
    }

    if (modeCount > 1)
    {
        fprintf(stderr, "Erro: selecione um modo por vez\n");
        return 1;
    }

    if (positions != -1 && modeHexX)
    {
       fprintf(stderr, "Erro: -p/--positions não pode ser usado com -x\n");
       return 1;
    }

    if (positions == -1){
       if (modeLinear)positions = 5;
        if (modeComb)positions = 3;
    }


        if (!key){
            if (!lerChaveDeArquivo("chave.txt", strCPUload))
            return 1;
        }

    if (modeHexX)
        if (!validarChaveHex(strCPUload, true)) // true permite x
            return 1;
        
    if (modeLinear || modeComb)
        if (!validarChaveHex(strCPUload, false)) //nao permite x
            return 1;



   if (showHelp){

    printf("\nUsage: %s [MODE] [OPTIONS]\n\n", argv[0]);

    printf("Modes:\n");
    printf("  -x, --hexx        Scattered HEX mode (use 'x' for unknown chars)\n");
    printf("  -l, --linear      Sequential scan of missing HEX characters\n");
    printf("  -c, --combination Test all combinations of unknown positions\n\n");

    printf("Options:\n");
    printf("  -p N              Unknown HEX positions (-c, -l modes only)\n");
    printf("  -k HEX            Partial key (otherwise reads 'chave.txt')\n");
    printf("  -h                Show this help message\n\n");

    printf("Example:\n");
    printf("  %s -x -k e3b0c44298fc1c149afbf4c8996fb92427ae41ex649b934ca49x991bx85xbx5x\n", argv[0]);
    printf("  %s -c -p 2 -k e3b0144298fc10149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n", argv[0]);
    printf("  %s -l -p 4 -k e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b78501235\n\n", argv[0]);

    printf("Donations: bc1qs850jrz5ktl5vwpma0sz40z29392wrzx9cevze\n");
    return 0;
   }


   increaseStackSizeCPU();
   mergeHashes(NAME_HASH_FOLDER, NAME_HASH_BUFFER);

   uint8_t* gTableXCPU = new uint8_t[COUNT_GTABLE_POINTS * SIZE_GTABLE_POINT];
   uint8_t* gTableYCPU = new uint8_t[COUNT_GTABLE_POINTS * SIZE_GTABLE_POINT];

   loadGTable(gTableXCPU, gTableYCPU);

   uint64_t* inputHashBufferCPU = new uint64_t[COUNT_INPUT_HASH];

   loadInputHash(inputHashBufferCPU);


    if (modeHexX)
        startSecp256k1ModeHexX(gTableXCPU, gTableYCPU, inputHashBufferCPU, strCPUload);
    if (modeLinear)
        startSecp256k1ModeHexScanL(gTableXCPU, gTableYCPU, inputHashBufferCPU, positions, strCPUload);
    if (modeComb)
        startSecp256k1ModeHexComb(gTableXCPU, gTableYCPU, inputHashBufferCPU, positions, strCPUload);

   delete[] gTableXCPU;
   delete[] gTableYCPU;
   delete[] inputHashBufferCPU;

   printf("Finish \n");
   return 0;
}
