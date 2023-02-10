

/************************************************************************************
** 
** Copyright 2021 stanzhao
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
** SOFTWARE.
** 
************************************************************************************/

void* MT19937_state32();

void  MT19937_close32(void* gen);

void  MT19937_srand32(void* gen, unsigned int seed);

unsigned int MT19937_random32(void* gen);

unsigned int MT19937_random32_range(void* gen, unsigned int low, unsigned int up);

void* MT19937_state64();

void  MT19937_close64(void* gen);

void  MT19937_srand64(void* gen, unsigned long long seed);

unsigned long long MT19937_random64(void* gen);

unsigned long long MT19937_random64_range(void* gen, unsigned long long low, unsigned long long up);

/***********************************************************************************/
