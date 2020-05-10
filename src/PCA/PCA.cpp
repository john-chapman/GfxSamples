#include "PCA.h"

#include <frm/core/frm.h>
#include <frm/core/rand.h>
#include <frm/core/ArgList.h>
#include <frm/core/Framebuffer.h>
#include <frm/core/GlContext.h>
#include <frm/core/Profiler.h>
#include <frm/core/Properties.h>
#include <frm/core/Shader.h>
#include <frm/core/Texture.h>

static PCA s_inst;

PCA::PCA()
	: AppBase("PCA") 
{
	Properties::PushGroup("PCA");
		//              name                         default                     min           max        storage
		Properties::Add("m_showGizmo",               m_showGizmo,                                         &m_showGizmo);
		Properties::Add("m_incrementalEstimateAvg",  m_incrementalEstimateAvg,                            &m_incrementalEstimateAvg);
		Properties::Add("m_dataCount",               m_dataCount,                2,            512,       &m_dataCount);
		Properties::Add("m_randomSeed",              m_randomSeed,               1,            999999,    &m_randomSeed);
		Properties::Add("m_varianceBounds",          m_varianceBounds,                                    &m_varianceBounds);
		Properties::Add("m_varianceGamma",           m_varianceGamma,            0.0f,         2.0f,      &m_varianceGamma);
	Properties::PopGroup();

}

PCA::~PCA()
{
	Properties::InvalidateGroup("PCA");
}

bool PCA::init(const ArgList& _args)
{
	if (!AppBase::init(_args)) 
	{
		return false;
	}

	m_worldMatrix = TransformationMatrix(vec3(0.5f), quat(0.0f, 0.0f, 0.0f, 1.0f), vec3(1.0f, 0.01f, 1.0f)); // put gizmo at the center of RGB space;

	Im3d::PushLayerId(Im3d::MakeId("background")); Im3d::PopLayerId();
	Im3d::PushLayerId(Im3d::MakeId("foreground")); Im3d::PopLayerId();

	return true;
}

void PCA::shutdown()
{
	shutdownData();

	AppBase::shutdown();
}

bool PCA::update()
{
	if (!AppBase::update()) 
	{
		return false;
	}

	bool reinit = !m_rawData;
	reinit |= ImGui::SliderInt("Data Count", &m_dataCount, 4, 512);
	reinit |= ImGui::SliderInt("Seed", &m_randomSeed, 1, 4096);
	if (reinit) 
	{
		initData();
	}

	ImGui::Checkbox("Incremental Estimate Avg", &m_incrementalEstimateAvg);
	
	ImGui::Checkbox("Show Gizmo", &m_showGizmo);
	if (m_showGizmo) 
	{
		Im3d::Gizmo("World Matrix", (float*)&m_worldMatrix);
	}
	for (int i = 0; i < m_dataCount; ++i) 
	{
		m_data[i] = TransformPosition(m_worldMatrix, m_rawData[i] - vec3(0.5f));
	}

	ImGui::Checkbox("Variance Bounds", &m_varianceBounds);
	if (m_varianceBounds)
	{
		ImGui::SliderFloat("Variance Gamma", &m_varianceGamma, 0.0f, 2.0f);
	}

 // compute min, max, avg + first and second moments
	vec3 dataMin, dataMax, dataAvg;
	dataMin = dataMax = dataAvg = m_data[0];
	vec3 dataM1 = m_data[0];
	vec3 dataM2 = dataM1 * dataM1;
	for (int i = 1; i < m_dataCount; ++i) 
	{
		dataMin  = Min(dataMin, m_data[i]);
		dataMax  = Max(dataMax, m_data[i]);
		dataAvg += m_data[i];
		dataM1  += m_data[i];
		dataM2  += m_data[i] * m_data[i];
	}
	dataAvg = dataAvg / (float)m_dataCount;

 // variance clipping
	vec3 mu = dataM1 / (float)m_dataCount;
	vec3 sigma = sqrt(dataM2 / (float)m_dataCount - mu * mu);
	vec3 ep0Variance = mu - m_varianceGamma * sigma;
	vec3 ep1Variance = mu + m_varianceGamma * sigma;

 // PCA batch
	vec3 axisBatch = pcaBatch(dataMin, dataMax, dataAvg);
	vec3 ep0Batch, ep1Batch;
	selectEndpoints(axisBatch, dataAvg, ep0Batch, ep1Batch);

 // PCA incremental
	vec3 axisIncremental = pcaIncremental(dataMin, dataMax, dataAvg);
	vec3 ep0Incremental, ep1Incremental;
	selectEndpoints(axisIncremental, dataAvg, ep0Incremental, ep1Incremental);


	Im3d::PushLayerId(Im3d::MakeId("background"));
		Im3d::PushDrawState();
		Im3d::SetSize(3.0f);
		Im3d::SetAlpha(0.25f);
		
		Im3d::SetColor(Im3d::Color_White);
		Im3d::DrawAlignedBox(vec3(0.0f), vec3(1.0f));

		Im3d::SetAlpha(0.5f);
		Im3d::DrawAlignedBox(dataMin, dataMax);
		
		Im3d::SetColor(Im3d::Color_Red);
		//Im3d::DrawAlignedBox(dataMin, dataMax);
		Im3d::BeginLines();
			Im3d::Vertex(dataAvg - vec3(0.1f, 0.0f, 0.0f));
			Im3d::Vertex(dataAvg + vec3(0.1f, 0.0f, 0.0f));
			Im3d::Vertex(dataAvg - vec3(0.0f, 0.1f, 0.0f));
			Im3d::Vertex(dataAvg + vec3(0.0f, 0.1f, 0.0f));
			Im3d::Vertex(dataAvg - vec3(0.0f, 0.0f, 0.1f));
			Im3d::Vertex(dataAvg + vec3(0.0f, 0.0f, 0.1f));
		Im3d::End();
		Im3d::PushMatrix(m_worldMatrix);
			Im3d::DrawSphere(vec3(0.0f), 0.5f, 256);
		Im3d::PopMatrix();

		Im3d::SetAlpha(1.0f);
		Im3d::BeginPoints();
			for (int i = 0; i < m_dataCount; ++i) 
			{
				Im3d::Vertex(m_data[i], 16.0f, Im3d::Color(m_data[i].x, m_data[i].y, m_data[i].z, 0.75f));
			}
		Im3d::End();
		Im3d::PopDrawState();
	Im3d::PopLayerId();

	Im3d::PushLayerId(Im3d::MakeId("foreground"));
		Im3d::PushDrawState();
		Im3d::SetAlpha(1.0f);
		Im3d::SetSize(8.0f);

		Im3d::SetColor(Im3d::Color_Magenta);
		Im3d::DrawArrow(ep0Batch, ep1Batch);

		Im3d::SetColor(Im3d::Color_Yellow);

		if (m_varianceBounds)
		{
			Im3d::DrawArrow(ep0Variance, ep1Variance);
		}
		else
		{
			Im3d::DrawArrow(ep0Incremental, ep1Incremental);
		}

		Im3d::PopDrawState();
	Im3d::PopLayerId();


	return true;
}

void PCA::draw()
{
 // code here

	AppBase::draw();
}

vec3 PCA::pcaBatch(const vec3& _min, const vec3& _max, const vec3& _avg)
{
 // compute the covariance matrix (it's symmetrical, only need 6 elements)
	float cov[6] = { 0.0f };
	for (int i = 0; i < m_dataCount; ++i) 
	{
		vec3 data = m_data[i] - _avg; // center the data on the mean
		
		cov[0] += data.x * data.x;
		cov[1] += data.x * data.y;
		cov[2] += data.x * data.z;
		cov[3] += data.y * data.y;
		cov[4] += data.y * data.z;
		cov[5] += data.z * data.z;
	}
	//vec3 axis = max - min;
	vec3 axis = vec3(0.9f, 1.0f, 0.7f); // from Crunch, more stable?

 // principle axis via power method
	for (int i = 0; i < 8; ++i) 
	{
		vec3 estimate = vec3(
			dot(axis, vec3(cov[0], cov[1], cov[2])),
			dot(axis, vec3(cov[1], cov[3], cov[4])),
			dot(axis, vec3(cov[2], cov[4], cov[5]))
			);

		float mx = Max(Abs(estimate.x), Max(Abs(estimate.y), Abs(estimate.z)));
		if (mx > 1e-7f) 
		{
			estimate = estimate * (1.0f / mx);
		}
		float delta = Length2(axis - estimate);
		axis = estimate;
		if (i > 2 && delta < 1e-6f) 
		{
			break;
		}
	}

	float palen = Length2(axis);
	if (palen> 1e-7f) 
	{
		axis = axis * (1.0f / sqrtf(palen));
	} 
	else 
	{
		axis = _max - _min;
	}

	return axis;
}

vec3 PCA::pcaIncremental(const vec3& _min, const vec3& _max, vec3& _avg_)
{
	vec3 axis = vec3(0.0f);
#if 0
 // https://arxiv.org/pdf/1511.03688.pdf
 // http://www.cse.msu.edu/~weng/research/CCIPCApami.pdf
	if (m_incrementalEstimateAvg) 
	{
		float amnesic =  2.0f;
		vec3  estAvg  = m_data[0];
			  estAvg  = ((1.0f - 1.0f - amnesic) / 1.0f) * estAvg + ((1.0f + amnesic) / 1.0f) * m_data[1];
		axis = m_data[1] - estAvg;
		for (int i = 2; i < m_dataCount; ++i) 
		{
			float n  = (float)i;
			float w0 = (n - 1.0f - amnesic) / n;
			float w1 = (1.0f + amnesic) / n;

			estAvg = w0 * estAvg + w1 * m_data[i]; 
			vec3 x = m_data[i] - estAvg;
			axis   = w0 * axis + w1 * dot(x, x) * normalize(axis);
		}

		ImGui::Text("Avg err: %f", Length(estAvg - _avg_));
		_avg_ = estAvg;
		
	} 
	else 
	{
		float amnesic = 0.0f;
		axis = m_data[0] - _avg_;
		for (int i = 1; i < m_dataCount; ++i) {
			float n  = (float)i;
			float w0 = (n - 1.0f - amnesic) / n;
			float w1 = (1.0f + amnesic) / n;

			vec3 x = m_data[i] - _avg_;
			axis   = w0 * axis + w1 * Length2(x) * normalize(axis);
		}
	}
	axis = normalize(axis);
#else
 // https://github.com/BinomialLLC/crunch/blob/master/crnlib/crn_dxt1.cpp
	if (m_incrementalEstimateAvg) 
	{
		vec3 estAvg = m_data[0];
		for (int i = 1; i < m_dataCount; ++i) 
		{
			float n = float(i);
			estAvg = n / (n + 1.0f) * estAvg + (1.0f / n) * m_data[i];
		}

		axis = m_data[0] - estAvg;
		for (int i = 1; i < m_dataCount; ++i) 
		{
			//float n = float(i + 1);
			//estAvg = n / (n + 1.0f) * estAvg + (1.0f / n) * m_data[i];
			vec3 data = m_data[i] - estAvg;

			vec3 x = data * data.x;
			vec3 y = data * data.y;
			vec3 z = data * data.z;
			
			vec3 v = normalize(axis);
			axis.x += dot(x, v);
			axis.y += dot(y, v);
			axis.z += dot(z, v);
		}

		ImGui::Text("Avg err: %f", Length(estAvg - _avg_));
		_avg_ = estAvg;

	} 
	else 
	{
		axis = m_data[0] - _avg_;
		for (int i = 1; i < m_dataCount; ++i) 
		{
			vec3 data = m_data[i] - _avg_;

			vec3 x = data * data.x;
			vec3 y = data * data.y;
			vec3 z = data * data.z;
			
			vec3 v = normalize(axis);
			axis.x += dot(x, v);
			axis.y += dot(y, v);
			axis.z += dot(z, v);
		}
	}

	axis = normalize(axis);
#endif

	return axis;
}

void PCA::selectEndpoints(const vec3& _axis, const vec3& _avg, vec3& ep0_, vec3& ep1_)
{
#if 0
 // project onto axis and choose the extrema (from stb_dxt)
	float mind, maxd;
	mind = maxd = dot(_axis, m_data[0] - _avg);
	for (int i = 1; i < m_dataCount; ++i)
	{
		float d = dot(_axis, m_data[i] - _avg);
		if (d < mind) 
		{
			ep0_ = m_data[i];
			mind = d;
		}
		if (d > maxd) 
		{
			ep1_ = m_data[i];
			maxd = d;
		}
	}
#else
 // less branchy
 // note that this is probably better as the result is guaranteed to pass through the mean, which in most cases should reduce overall error
	float mind, maxd;
	mind = maxd = dot(_axis, m_data[0] - _avg);
	for (int i = 1; i < m_dataCount; ++i) 
	{
		float d = dot(_axis, m_data[i] - _avg);
		mind = Min(mind, d);
		maxd = Max(maxd, d);
	}
	ep0_ = _avg + mind * _axis;
	ep1_ = _avg + maxd * _axis;
#endif

	ep0_ = Saturate(ep0_);
	ep1_ = Saturate(ep1_);
}

void PCA::initData()
{
	shutdownData();

	m_rawData = new vec3[m_dataCount];
	m_data    = new vec3[m_dataCount];

	Rand<> rnd((uint32)m_randomSeed);
	for (int i = 0; i < m_dataCount; ++i) 
	{
	#if 0
	 // sphere
		m_rawData[i] = rnd.get<vec3>(vec3(-1.0f), vec3(1.0f));
		m_rawData[i] = normalize(m_rawData[i]) * rnd.get<float>(0.0f, 0.5f) + vec3(0.5f);
	#else
	 // cube
		m_rawData[i] = rnd.get<vec3>(vec3(0.0f), vec3(1.0f));
	#endif
	}
}

void PCA::shutdownData()
{
	delete m_rawData;
	delete m_data;
}
	