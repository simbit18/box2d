// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#include "draw.h"
#include "random.h"
#include "sample.h"

#include "box2d/box2d.h"
#include "box2d/collision.h"
#include "box2d/math_functions.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <stdlib.h>

class ShapeDistance : public Sample
{
public:
	enum ShapeType
	{
		e_point,
		e_segment,
		e_triangle,
		e_box
	};

	explicit ShapeDistance( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 0.0f, 0.0f };
			m_context->camera.m_zoom = 3.0f;
		}

		m_point = b2Vec2_zero;
		m_segment = { { -0.5f, 0.0f }, { 0.5f, 0.0f } };

		{
			b2Vec2 points[3] = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 1.0f } };
			b2Hull hull = b2ComputeHull( points, 3 );
			m_triangle = b2MakePolygon( &hull, 0.0f );

			// m_triangle = b2MakeSquare( 0.4f );
		}

		m_box = b2MakeSquare( 0.5f );

		// m_transform = { { 1.5f, -1.5f }, b2Rot_identity };
		m_transform = { { 0.0f, 0.0f }, b2Rot_identity };
		m_angle = 0.0f;

		m_cache = b2_emptySimplexCache;
		m_simplexCount = 0;
		m_startPoint = { 0.0f, 0.0f };
		m_basePosition = { 0.0f, 0.0f };
		m_baseAngle = 0.0f;

		m_dragging = false;
		m_rotating = false;
		m_showIndices = false;
		m_useCache = false;
		m_drawSimplex = false;

		m_typeA = e_box;
		m_typeB = e_box;
		m_radiusA = 0.0f;
		m_radiusB = 0.0f;

		m_proxyA = MakeProxy( m_typeA, m_radiusA );
		m_proxyB = MakeProxy( m_typeB, m_radiusB );
	}

	b2ShapeProxy MakeProxy( ShapeType type, float radius )
	{
		b2ShapeProxy proxy = {};
		proxy.radius = radius;

		switch ( type )
		{
			case e_point:
				proxy.points[0] = b2Vec2_zero;
				proxy.count = 1;
				break;

			case e_segment:
				proxy.points[0] = m_segment.point1;
				proxy.points[1] = m_segment.point2;
				proxy.count = 2;
				break;

			case e_triangle:
				for ( int i = 0; i < m_triangle.count; ++i )
				{
					proxy.points[i] = m_triangle.vertices[i];
				}
				proxy.count = m_triangle.count;
				break;

			case e_box:
				proxy.points[0] = m_box.vertices[0];
				proxy.points[1] = m_box.vertices[1];
				proxy.points[2] = m_box.vertices[2];
				proxy.points[3] = m_box.vertices[3];
				proxy.count = 4;
				break;

			default:
				assert( false );
		}

		return proxy;
	}

	void DrawShape( ShapeType type, b2Transform transform, float radius, b2HexColor color )
	{
		switch ( type )
		{
			case e_point:
			{
				b2Vec2 p = b2TransformPoint( transform, m_point );
				if ( radius > 0.0f )
				{
					m_context->draw.DrawSolidCircle( transform, m_point, radius, color );
				}
				else
				{
					m_context->draw.DrawPoint( p, 5.0f, color );
				}
			}
			break;

			case e_segment:
			{
				b2Vec2 p1 = b2TransformPoint( transform, m_segment.point1 );
				b2Vec2 p2 = b2TransformPoint( transform, m_segment.point2 );

				if ( radius > 0.0f )
				{
					m_context->draw.DrawSolidCapsule( p1, p2, radius, color );
				}
				else
				{
					m_context->draw.DrawLine( p1, p2, color );
				}
			}
			break;

			case e_triangle:
				m_context->draw.DrawSolidPolygon( transform, m_triangle.vertices, m_triangle.count, radius, color );
				break;

			case e_box:
				m_context->draw.DrawSolidPolygon( transform, m_box.vertices, m_box.count, radius, color );
				break;

			default:
				assert( false );
		}
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 310.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 240.0f, height ) );

		ImGui::Begin( "Shape Distance", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		const char* shapeTypes[] = { "point", "segment", "triangle", "box" };
		int shapeType = int( m_typeA );
		if ( ImGui::Combo( "shape A", &shapeType, shapeTypes, IM_ARRAYSIZE( shapeTypes ) ) )
		{
			m_typeA = ShapeType( shapeType );
			m_proxyA = MakeProxy( m_typeA, m_radiusA );
		}

		if ( ImGui::SliderFloat( "radius A", &m_radiusA, 0.0f, 0.5f, "%.2f" ) )
		{
			m_proxyA.radius = m_radiusA;
		}

		shapeType = int( m_typeB );
		if ( ImGui::Combo( "shape B", &shapeType, shapeTypes, IM_ARRAYSIZE( shapeTypes ) ) )
		{
			m_typeB = ShapeType( shapeType );
			m_proxyB = MakeProxy( m_typeB, m_radiusB );
		}

		if ( ImGui::SliderFloat( "radius B", &m_radiusB, 0.0f, 0.5f, "%.2f" ) )
		{
			m_proxyB.radius = m_radiusB;
		}

		ImGui::Separator();

		ImGui::SliderFloat( "x offset", &m_transform.p.x, -2.0f, 2.0f, "%.2f" );
		ImGui::SliderFloat( "y offset", &m_transform.p.y, -2.0f, 2.0f, "%.2f" );

		if ( ImGui::SliderFloat( "angle", &m_angle, -B2_PI, B2_PI, "%.2f" ) )
		{
			m_transform.q = b2MakeRot( m_angle );
		}

		ImGui::Separator();

		ImGui::Checkbox( "show indices", &m_showIndices );
		ImGui::Checkbox( "use cache", &m_useCache );

		ImGui::Separator();

		if ( ImGui::Checkbox( "draw simplex", &m_drawSimplex ) )
		{
			m_simplexIndex = 0;
		}

		if ( m_drawSimplex )
		{
			ImGui::SliderInt( "index", &m_simplexIndex, 0, m_simplexCount - 1 );
			m_simplexIndex = b2ClampInt( m_simplexIndex, 0, m_simplexCount - 1 );
		}

		ImGui::End();
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 && m_rotating == false )
			{
				m_dragging = true;
				m_startPoint = p;
				m_basePosition = m_transform.p;
			}
			else if ( mods == GLFW_MOD_SHIFT && m_dragging == false )
			{
				m_rotating = true;
				m_startPoint = p;
				m_baseAngle = m_angle;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_dragging = false;
			m_rotating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_dragging )
		{
			m_transform.p = m_basePosition + 0.5f * ( p - m_startPoint );
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_startPoint.x;
			m_angle = b2ClampFloat( m_baseAngle + 1.0f * dx, -B2_PI, B2_PI );
			m_transform.q = b2MakeRot( m_angle );
		}
	}

	static b2Vec2 Weight2( float a1, b2Vec2 w1, float a2, b2Vec2 w2 )
	{
		return { a1 * w1.x + a2 * w2.x, a1 * w1.y + a2 * w2.y };
	}

	static b2Vec2 Weight3( float a1, b2Vec2 w1, float a2, b2Vec2 w2, float a3, b2Vec2 w3 )
	{
		return { a1 * w1.x + a2 * w2.x + a3 * w3.x, a1 * w1.y + a2 * w2.y + a3 * w3.y };
	}

	void ComputeSimplexWitnessPoints( b2Vec2* a, b2Vec2* b, const b2Simplex* s )
	{
		switch ( s->count )
		{
			case 0:
				assert( false );
				break;

			case 1:
				*a = s->v1.wA;
				*b = s->v1.wB;
				break;

			case 2:
				*a = Weight2( s->v1.a, s->v1.wA, s->v2.a, s->v2.wA );
				*b = Weight2( s->v1.a, s->v1.wB, s->v2.a, s->v2.wB );
				break;

			case 3:
				*a = Weight3( s->v1.a, s->v1.wA, s->v2.a, s->v2.wA, s->v3.a, s->v3.wA );
				*b = *a;
				break;

			default:
				assert( false );
				break;
		}
	}

	void Step() override
	{
		b2DistanceInput input;
		input.proxyA = m_proxyA;
		input.proxyB = m_proxyB;
		input.transformA = b2Transform_identity;
		input.transformB = m_transform;
		input.useRadii = true || m_radiusA > 0.0f || m_radiusB > 0.0f;

		if ( m_useCache == false )
		{
			m_cache.count = 0;
		}

		b2DistanceOutput output = b2ShapeDistance( &input, &m_cache, m_simplexes, m_simplexCapacity );

		m_simplexCount = output.simplexCount;

		DrawShape( m_typeA, b2Transform_identity, m_radiusA, b2_colorCyan );
		DrawShape( m_typeB, m_transform, m_radiusB, b2_colorBisque );

		if ( m_drawSimplex )
		{
			b2Simplex* simplex = m_simplexes + m_simplexIndex;
			b2SimplexVertex* vertices[3] = { &simplex->v1, &simplex->v2, &simplex->v3 };

			if ( m_simplexIndex > 0 )
			{
				// The first recorded simplex does not have valid barycentric coordinates
				b2Vec2 pointA, pointB;
				ComputeSimplexWitnessPoints( &pointA, &pointB, simplex );

				m_context->draw.DrawLine( pointA, pointB, b2_colorWhite );
				m_context->draw.DrawPoint( pointA, 10.0f, b2_colorWhite );
				m_context->draw.DrawPoint( pointB, 10.0f, b2_colorWhite );
			}

			b2HexColor colors[3] = { b2_colorRed, b2_colorGreen, b2_colorBlue };

			for ( int i = 0; i < simplex->count; ++i )
			{
				b2SimplexVertex* vertex = vertices[i];
				m_context->draw.DrawPoint( vertex->wA, 10.0f, colors[i] );
				m_context->draw.DrawPoint( vertex->wB, 10.0f, colors[i] );
			}
		}
		else
		{
			m_context->draw.DrawLine( output.pointA, output.pointB, b2_colorDimGray );
			m_context->draw.DrawPoint( output.pointA, 10.0f, b2_colorWhite );
			m_context->draw.DrawPoint( output.pointB, 10.0f, b2_colorWhite );

			m_context->draw.DrawLine( output.pointA, output.pointA + 0.5f * output.normal, b2_colorYellow );
		}

		if ( m_showIndices )
		{
			for ( int i = 0; i < m_proxyA.count; ++i )
			{
				b2Vec2 p = m_proxyA.points[i];
				m_context->draw.DrawString( p, " %d", i );
			}

			for ( int i = 0; i < m_proxyB.count; ++i )
			{
				b2Vec2 p = b2TransformPoint( m_transform, m_proxyB.points[i] );
				m_context->draw.DrawString( p, " %d", i );
			}
		}

		DrawTextLine( "mouse button 1: drag" );
		DrawTextLine( "mouse button 1 + shift: rotate" );
		DrawTextLine( "distance = %.2f, iterations = %d", output.distance, output.iterations );

		if ( m_cache.count == 1 )
		{
			DrawTextLine( "cache = {%d}, {%d}", m_cache.indexA[0], m_cache.indexB[0] );
		}
		else if ( m_cache.count == 2 )
		{
			DrawTextLine( "cache = {%d, %d}, {%d, %d}", m_cache.indexA[0], m_cache.indexA[1], m_cache.indexB[0],
						  m_cache.indexB[1] );
		}
		else if ( m_cache.count == 3 )
		{
			DrawTextLine( "cache = {%d, %d, %d}, {%d, %d, %d}", m_cache.indexA[0], m_cache.indexA[1], m_cache.indexA[2],
						  m_cache.indexB[0], m_cache.indexB[1], m_cache.indexB[2] );
		}
	}

	static Sample* Create( SampleContext* context )
	{
		return new ShapeDistance( context );
	}

	static constexpr int m_simplexCapacity = 20;

	b2Polygon m_box;
	b2Polygon m_triangle;
	b2Vec2 m_point;
	b2Segment m_segment;

	ShapeType m_typeA;
	ShapeType m_typeB;
	float m_radiusA;
	float m_radiusB;
	b2ShapeProxy m_proxyA;
	b2ShapeProxy m_proxyB;

	b2SimplexCache m_cache;
	b2Simplex m_simplexes[m_simplexCapacity];
	int m_simplexCount;
	int m_simplexIndex;

	b2Transform m_transform;
	float m_angle;

	b2Vec2 m_basePosition;
	b2Vec2 m_startPoint;
	float m_baseAngle;

	bool m_dragging;
	bool m_rotating;
	bool m_showIndices;
	bool m_useCache;
	bool m_drawSimplex;
};

static int sampleShapeDistance = RegisterSample( "Collision", "Shape Distance", ShapeDistance::Create );

enum UpdateType
{
	Update_Incremental = 0,
	Update_FullRebuild = 1,
	Update_PartialRebuild = 2,
};

struct Proxy
{
	b2AABB box;
	b2AABB fatBox;
	b2Vec2 position;
	b2Vec2 width;
	int proxyId;
	int rayStamp;
	int queryStamp;
	bool moved;
};

static bool QueryCallback( int32_t proxyId, uint64_t userData, void* context );
static float RayCallback( const b2RayCastInput* input, int32_t proxyId, uint64_t userData, void* context );

// Tests the Box2D bounding volume hierarchy (BVH). The dynamic tree
// can be used independently as a spatial data structure.
class DynamicTree : public Sample
{
public:
	explicit DynamicTree( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 500.0f, 500.0f };
			m_context->camera.m_zoom = 25.0f * 21.0f;
		}

		m_fill = 0.25f;
		m_moveFraction = 0.05f;
		m_moveDelta = 0.1f;
		m_proxies = nullptr;
		m_proxyCount = 0;
		m_proxyCapacity = 0;
		m_ratio = 5.0f;
		m_grid = 1.0f;

		m_moveBuffer = nullptr;
		m_moveCount = 0;

		m_rowCount = m_isDebug ? 100 : 1000;
		m_columnCount = m_isDebug ? 100 : 1000;
		memset( &m_tree, 0, sizeof( m_tree ) );
		BuildTree();
		m_timeStamp = 0;
		m_updateType = Update_Incremental;

		m_startPoint = { 0.0f, 0.0f };
		m_endPoint = { 0.0f, 0.0f };
		m_queryDrag = false;
		m_rayDrag = false;
		m_validate = true;
	}

	~DynamicTree() override
	{
		free( m_proxies );
		free( m_moveBuffer );
		b2DynamicTree_Destroy( &m_tree );
	}

	void BuildTree()
	{
		b2DynamicTree_Destroy( &m_tree );
		free( m_proxies );
		free( m_moveBuffer );

		m_proxyCapacity = m_rowCount * m_columnCount;
		m_proxies = static_cast<Proxy*>( malloc( m_proxyCapacity * sizeof( Proxy ) ) );
		m_proxyCount = 0;

		m_moveBuffer = static_cast<int*>( malloc( m_proxyCapacity * sizeof( int ) ) );
		m_moveCount = 0;

		float y = -4.0f;

		m_tree = b2DynamicTree_Create();

		const b2Vec2 aabbMargin = { 0.1f, 0.1f };

		for ( int i = 0; i < m_rowCount; ++i )
		{
			float x = -40.0f;

			for ( int j = 0; j < m_columnCount; ++j )
			{
				float fillTest = RandomFloatRange( 0.0f, 1.0f );
				if ( fillTest <= m_fill )
				{
					assert( m_proxyCount <= m_proxyCapacity );
					Proxy* p = m_proxies + m_proxyCount;
					p->position = { x, y };

					float ratio = RandomFloatRange( 1.0f, m_ratio );
					float width = RandomFloatRange( 0.1f, 0.5f );
					if ( RandomFloat() > 0.0f )
					{
						p->width.x = ratio * width;
						p->width.y = width;
					}
					else
					{
						p->width.x = width;
						p->width.y = ratio * width;
					}

					p->box.lowerBound = { x, y };
					p->box.upperBound = { x + p->width.x, y + p->width.y };
					p->fatBox.lowerBound = b2Sub( p->box.lowerBound, aabbMargin );
					p->fatBox.upperBound = b2Add( p->box.upperBound, aabbMargin );

					p->proxyId = b2DynamicTree_CreateProxy( &m_tree, p->fatBox, B2_DEFAULT_CATEGORY_BITS, m_proxyCount );
					p->rayStamp = -1;
					p->queryStamp = -1;
					p->moved = false;
					++m_proxyCount;
				}

				x += m_grid;
			}

			y += m_grid;
		}
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 320.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 200.0f, height ) );

		ImGui::Begin( "Dynamic Tree", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		ImGui::PushItemWidth( 100.0f );

		bool changed = false;
		if ( ImGui::SliderInt( "rows", &m_rowCount, 0, 1000, "%d" ) )
		{
			changed = true;
		}

		if ( ImGui::SliderInt( "columns", &m_columnCount, 0, 1000, "%d" ) )
		{
			changed = true;
		}

		if ( ImGui::SliderFloat( "fill", &m_fill, 0.0f, 1.0f, "%.2f" ) )
		{
			changed = true;
		}

		if ( ImGui::SliderFloat( "grid", &m_grid, 0.5f, 2.0f, "%.2f" ) )
		{
			changed = true;
		}

		if ( ImGui::SliderFloat( "ratio", &m_ratio, 1.0f, 10.0f, "%.2f" ) )
		{
			changed = true;
		}

		if ( ImGui::SliderFloat( "move", &m_moveFraction, 0.0f, 1.0f, "%.2f" ) )
		{
		}

		if ( ImGui::SliderFloat( "delta", &m_moveDelta, 0.0f, 1.0f, "%.2f" ) )
		{
		}

		if ( ImGui::RadioButton( "Incremental", m_updateType == Update_Incremental ) )
		{
			m_updateType = Update_Incremental;
			changed = true;
		}

		if ( ImGui::RadioButton( "Full Rebuild", m_updateType == Update_FullRebuild ) )
		{
			m_updateType = Update_FullRebuild;
			changed = true;
		}

		if ( ImGui::RadioButton( "Partial Rebuild", m_updateType == Update_PartialRebuild ) )
		{
			m_updateType = Update_PartialRebuild;
			changed = true;
		}

		ImGui::Separator();

		ImGui::Text( "mouse button 1: ray cast" );
		ImGui::Text( "mouse button 1 + shift: query" );

		ImGui::PopItemWidth();
		ImGui::End();

		if ( changed )
		{
			BuildTree();
		}
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 && m_queryDrag == false )
			{
				m_rayDrag = true;
				m_startPoint = p;
				m_endPoint = p;
			}
			else if ( mods == GLFW_MOD_SHIFT && m_rayDrag == false )
			{
				m_queryDrag = true;
				m_startPoint = p;
				m_endPoint = p;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_queryDrag = false;
			m_rayDrag = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		m_endPoint = p;
	}

	void Step() override
	{
		if ( m_queryDrag )
		{
			b2AABB box = { b2Min( m_startPoint, m_endPoint ), b2Max( m_startPoint, m_endPoint ) };
			b2DynamicTree_Query( &m_tree, box, B2_DEFAULT_MASK_BITS, QueryCallback, this );

			m_context->draw.DrawBounds( box, b2_colorWhite );
		}

		// m_startPoint = {-1.0f, 0.5f};
		// m_endPoint = {7.0f, 0.5f};

		if ( m_rayDrag )
		{
			b2RayCastInput input = { m_startPoint, b2Sub( m_endPoint, m_startPoint ), 1.0f };
			b2TreeStats result = b2DynamicTree_RayCast( &m_tree, &input, B2_DEFAULT_MASK_BITS, RayCallback, this );

			m_context->draw.DrawLine( m_startPoint, m_endPoint, b2_colorWhite );
			m_context->draw.DrawPoint( m_startPoint, 5.0f, b2_colorGreen );
			m_context->draw.DrawPoint( m_endPoint, 5.0f, b2_colorRed );

			DrawTextLine( "node visits = %d, leaf visits = %d", result.nodeVisits, result.leafVisits );
		}

		b2HexColor c = b2_colorBlue;
		b2HexColor qc = b2_colorGreen;

		const b2Vec2 aabbMargin = { 0.1f, 0.1f };

		for ( int i = 0; i < m_proxyCount; ++i )
		{
			Proxy* p = m_proxies + i;

			if ( p->queryStamp == m_timeStamp || p->rayStamp == m_timeStamp )
			{
				m_context->draw.DrawBounds( p->box, qc );
			}
			else
			{
				m_context->draw.DrawBounds( p->box, c );
			}

			float moveTest = RandomFloatRange( 0.0f, 1.0f );
			if ( m_moveFraction > moveTest )
			{
				float dx = m_moveDelta * RandomFloat();
				float dy = m_moveDelta * RandomFloat();

				p->position.x += dx;
				p->position.y += dy;

				p->box.lowerBound.x = p->position.x + dx;
				p->box.lowerBound.y = p->position.y + dy;
				p->box.upperBound.x = p->position.x + dx + p->width.x;
				p->box.upperBound.y = p->position.y + dy + p->width.y;

				if ( b2AABB_Contains( p->fatBox, p->box ) == false )
				{
					p->fatBox.lowerBound = b2Sub( p->box.lowerBound, aabbMargin );
					p->fatBox.upperBound = b2Add( p->box.upperBound, aabbMargin );
					p->moved = true;
				}
				else
				{
					p->moved = false;
				}
			}
			else
			{
				p->moved = false;
			}
		}

		switch ( m_updateType )
		{
			case Update_Incremental:
			{
				uint64_t ticks = b2GetTicks();
				for ( int i = 0; i < m_proxyCount; ++i )
				{
					Proxy* p = m_proxies + i;
					if ( p->moved )
					{
						b2DynamicTree_MoveProxy( &m_tree, p->proxyId, p->fatBox );
					}
				}
				float ms = b2GetMilliseconds( ticks );
				DrawTextLine( "incremental : %.3f ms", ms );
			}
			break;

			case Update_FullRebuild:
			{
				for ( int i = 0; i < m_proxyCount; ++i )
				{
					Proxy* p = m_proxies + i;
					if ( p->moved )
					{
						b2DynamicTree_EnlargeProxy( &m_tree, p->proxyId, p->fatBox );
					}
				}

				uint64_t ticks = b2GetTicks();
				int boxCount = b2DynamicTree_Rebuild( &m_tree, true );
				float ms = b2GetMilliseconds( ticks );
				DrawTextLine( "full build %d : %.3f ms", boxCount, ms );
			}
			break;

			case Update_PartialRebuild:
			{
				for ( int i = 0; i < m_proxyCount; ++i )
				{
					Proxy* p = m_proxies + i;
					if ( p->moved )
					{
						b2DynamicTree_EnlargeProxy( &m_tree, p->proxyId, p->fatBox );
					}
				}

				uint64_t ticks = b2GetTicks();
				int boxCount = b2DynamicTree_Rebuild( &m_tree, false );
				float ms = b2GetMilliseconds( ticks );
				DrawTextLine( "partial rebuild %d : %.3f ms", boxCount, ms );
			}
			break;

			default:
				break;
		}

		int height = b2DynamicTree_GetHeight( &m_tree );
		float areaRatio = b2DynamicTree_GetAreaRatio( &m_tree );

		int hmin = (int)( ceilf( logf( (float)m_proxyCount ) / logf( 2.0f ) - 1.0f ) );
		DrawTextLine( "proxies = %d, height = %d, hmin = %d, area ratio = %.1f", m_proxyCount, height, hmin, areaRatio );

		b2DynamicTree_Validate( &m_tree );

		m_timeStamp += 1;
	}

	static Sample* Create( SampleContext* context )
	{
		return new DynamicTree( context );
	}

	b2DynamicTree m_tree;
	int m_rowCount, m_columnCount;
	Proxy* m_proxies;
	int* m_moveBuffer;
	int m_moveCount;
	int m_proxyCapacity;
	int m_proxyCount;
	int m_timeStamp;
	int m_updateType;
	float m_fill;
	float m_moveFraction;
	float m_moveDelta;
	float m_ratio;
	float m_grid;

	b2Vec2 m_startPoint;
	b2Vec2 m_endPoint;

	bool m_rayDrag;
	bool m_queryDrag;
	bool m_validate;
};

static bool QueryCallback( int proxyId, uint64_t userData, void* context )
{
	DynamicTree* sample = static_cast<DynamicTree*>( context );
	Proxy* proxy = sample->m_proxies + userData;
	assert( proxy->proxyId == proxyId );
	proxy->queryStamp = sample->m_timeStamp;
	return true;
}

static float RayCallback( const b2RayCastInput* input, int proxyId, uint64_t userData, void* context )
{
	DynamicTree* sample = static_cast<DynamicTree*>( context );
	Proxy* proxy = sample->m_proxies + userData;
	assert( proxy->proxyId == proxyId );
	proxy->rayStamp = sample->m_timeStamp;
	return input->maxFraction;
}

static int sampleDynamicTree = RegisterSample( "Collision", "Dynamic Tree", DynamicTree::Create );

class RayCast : public Sample
{
public:
	explicit RayCast( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 0.0f, 20.0f };
			m_context->camera.m_zoom = 17.5f;
		}

		m_circle = { { 0.0f, 0.0f }, 2.0f };
		m_capsule = { { -1.0f, 1.0f }, { 1.0f, -1.0f }, 1.5f };
		m_box = b2MakeBox( 2.0f, 2.0f );

		b2Vec2 vertices[3] = { { -2.0f, 0.0f }, { 2.0f, 0.0f }, { 2.0f, 3.0f } };
		b2Hull hull = b2ComputeHull( vertices, 3 );
		m_triangle = b2MakePolygon( &hull, 0.0f );

		m_segment = { { -3.0f, 0.0f }, { 3.0f, 0.0f } };

		m_transform = b2Transform_identity;
		m_angle = 0.0f;

		m_basePosition = { 0.0f, 0.0f };
		m_baseAngle = 0.0f;
		m_startPosition = { 0.0f, 0.0f };

		m_rayStart = { 0.0f, 30.0f };
		m_rayEnd = { 0.0f, 0.0f };

		m_rayDrag = false;
		m_translating = false;
		m_rotating = false;

		m_showFraction = false;
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 230.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 200.0f, height ) );

		ImGui::Begin( "Ray-cast", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		ImGui::PushItemWidth( 100.0f );

		ImGui::SliderFloat( "x offset", &m_transform.p.x, -2.0f, 2.0f, "%.2f" );
		ImGui::SliderFloat( "y offset", &m_transform.p.y, -2.0f, 2.0f, "%.2f" );

		if ( ImGui::SliderFloat( "angle", &m_angle, -B2_PI, B2_PI, "%.2f" ) )
		{
			m_transform.q = b2MakeRot( m_angle );
		}

		// if (ImGui::SliderFloat("ray radius", &m_rayRadius, 0.0f, 1.0f, "%.1f"))
		//{
		// }

		ImGui::Checkbox( "show fraction", &m_showFraction );

		if ( ImGui::Button( "Reset" ) )
		{
			m_transform = b2Transform_identity;
			m_angle = 0.0f;
		}

		ImGui::Separator();

		ImGui::Text( "mouse btn 1: ray cast" );
		ImGui::Text( "mouse btn 1 + shft: translate" );
		ImGui::Text( "mouse btn 1 + ctrl: rotate" );

		ImGui::PopItemWidth();

		ImGui::End();
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_startPosition = p;

			if ( mods == 0 )
			{
				m_rayStart = p;
				m_rayDrag = true;
			}
			else if ( mods == GLFW_MOD_SHIFT )
			{
				m_translating = true;
				m_basePosition = m_transform.p;
			}
			else if ( mods == GLFW_MOD_CONTROL )
			{
				m_rotating = true;
				m_baseAngle = m_angle;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_rayDrag = false;
			m_rotating = false;
			m_translating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_rayDrag )
		{
			m_rayEnd = p;
		}
		else if ( m_translating )
		{
			m_transform.p.x = m_basePosition.x + 0.5f * ( p.x - m_startPosition.x );
			m_transform.p.y = m_basePosition.y + 0.5f * ( p.y - m_startPosition.y );
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_startPosition.x;
			m_angle = b2ClampFloat( m_baseAngle + 0.5f * dx, -B2_PI, B2_PI );
			m_transform.q = b2MakeRot( m_angle );
		}
	}

	void DrawRay( const b2CastOutput* output )
	{
		b2Vec2 p1 = m_rayStart;
		b2Vec2 p2 = m_rayEnd;
		b2Vec2 d = b2Sub( p2, p1 );

		if ( output->hit )
		{
			b2Vec2 p;

			if ( output->fraction == 0.0f )
			{
				assert( output->normal.x == 0.0f && output->normal.y == 0.0f );
				p = output->point;
				m_draw->DrawPoint( output->point, 5.0, b2_colorPeru );
			}
			else
			{
				p = b2MulAdd( p1, output->fraction, d );
				m_draw->DrawLine( p1, p, b2_colorWhite );
				m_draw->DrawPoint( p1, 5.0f, b2_colorGreen );
				m_draw->DrawPoint( output->point, 5.0f, b2_colorWhite );

				b2Vec2 n = b2MulAdd( p, 1.0f, output->normal );
				m_draw->DrawLine( p, n, b2_colorViolet );
			}

			if ( m_showFraction )
			{
				b2Vec2 ps = { p.x + 0.05f, p.y - 0.02f };
				m_draw->DrawString( ps, "%.2f", output->fraction );
			}
		}
		else
		{
			m_draw->DrawLine( p1, p2, b2_colorWhite );
			m_draw->DrawPoint( p1, 5.0f, b2_colorGreen );
			m_draw->DrawPoint( p2, 5.0f, b2_colorRed );
		}
	}

	void Step() override
	{
		b2Vec2 offset = { -20.0f, 20.0f };
		b2Vec2 increment = { 10.0f, 0.0f };

		b2HexColor color1 = b2_colorYellow;

		b2CastOutput output = {};
		float maxFraction = 1.0f;

		// circle
		{
			b2Transform transform = { b2Add( m_transform.p, offset ), m_transform.q };
			m_context->draw.DrawSolidCircle( transform, m_circle.center, m_circle.radius, color1 );

			b2Vec2 start = b2InvTransformPoint( transform, m_rayStart );
			b2Vec2 translation = b2InvRotateVector( transform.q, b2Sub( m_rayEnd, m_rayStart ) );
			b2RayCastInput input = { start, translation, maxFraction };

			b2CastOutput localOutput = b2RayCastCircle( &m_circle, &input );
			if ( localOutput.hit )
			{
				output = localOutput;
				output.point = b2TransformPoint( transform, localOutput.point );
				output.normal = b2RotateVector( transform.q, localOutput.normal );
				maxFraction = localOutput.fraction;
			}

			offset = b2Add( offset, increment );
		}

		// capsule
		{
			b2Transform transform = { b2Add( m_transform.p, offset ), m_transform.q };
			b2Vec2 v1 = b2TransformPoint( transform, m_capsule.center1 );
			b2Vec2 v2 = b2TransformPoint( transform, m_capsule.center2 );
			m_context->draw.DrawSolidCapsule( v1, v2, m_capsule.radius, color1 );

			b2Vec2 start = b2InvTransformPoint( transform, m_rayStart );
			b2Vec2 translation = b2InvRotateVector( transform.q, b2Sub( m_rayEnd, m_rayStart ) );
			b2RayCastInput input = { start, translation, maxFraction };

			b2CastOutput localOutput = b2RayCastCapsule( &m_capsule, &input );
			if ( localOutput.hit )
			{
				output = localOutput;
				output.point = b2TransformPoint( transform, localOutput.point );
				output.normal = b2RotateVector( transform.q, localOutput.normal );
				maxFraction = localOutput.fraction;
			}

			offset = b2Add( offset, increment );
		}

		// box
		{
			b2Transform transform = { b2Add( m_transform.p, offset ), m_transform.q };
			m_context->draw.DrawSolidPolygon( transform, m_box.vertices, m_box.count, 0.0f, color1 );

			b2Vec2 start = b2InvTransformPoint( transform, m_rayStart );
			b2Vec2 translation = b2InvRotateVector( transform.q, b2Sub( m_rayEnd, m_rayStart ) );
			b2RayCastInput input = { start, translation, maxFraction };

			b2CastOutput localOutput = b2RayCastPolygon( &m_box, &input );
			if ( localOutput.hit )
			{
				output = localOutput;
				output.point = b2TransformPoint( transform, localOutput.point );
				output.normal = b2RotateVector( transform.q, localOutput.normal );
				maxFraction = localOutput.fraction;
			}

			offset = b2Add( offset, increment );
		}

		// triangle
		{
			b2Transform transform = { b2Add( m_transform.p, offset ), m_transform.q };
			m_context->draw.DrawSolidPolygon( transform, m_triangle.vertices, m_triangle.count, 0.0f, color1 );

			b2Vec2 start = b2InvTransformPoint( transform, m_rayStart );
			b2Vec2 translation = b2InvRotateVector( transform.q, b2Sub( m_rayEnd, m_rayStart ) );
			b2RayCastInput input = { start, translation, maxFraction };

			b2CastOutput localOutput = b2RayCastPolygon( &m_triangle, &input );
			if ( localOutput.hit )
			{
				output = localOutput;
				output.point = b2TransformPoint( transform, localOutput.point );
				output.normal = b2RotateVector( transform.q, localOutput.normal );
				maxFraction = localOutput.fraction;
			}

			offset = b2Add( offset, increment );
		}

		// segment
		{
			b2Transform transform = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Vec2 p1 = b2TransformPoint( transform, m_segment.point1 );
			b2Vec2 p2 = b2TransformPoint( transform, m_segment.point2 );
			m_context->draw.DrawLine( p1, p2, color1 );

			b2Vec2 start = b2InvTransformPoint( transform, m_rayStart );
			b2Vec2 translation = b2InvRotateVector( transform.q, b2Sub( m_rayEnd, m_rayStart ) );
			b2RayCastInput input = { start, translation, maxFraction };

			b2CastOutput localOutput = b2RayCastSegment( &m_segment, &input, false );
			if ( localOutput.hit )
			{
				output = localOutput;
				output.point = b2TransformPoint( transform, localOutput.point );
				output.normal = b2RotateVector( transform.q, localOutput.normal );
				maxFraction = localOutput.fraction;
			}

			offset = b2Add( offset, increment );
		}

		DrawRay( &output );
	}

	static Sample* Create( SampleContext* context )
	{
		return new RayCast( context );
	}

	b2Polygon m_box;
	b2Polygon m_triangle;
	b2Circle m_circle;
	b2Capsule m_capsule;
	b2Segment m_segment;

	b2Transform m_transform;
	float m_angle;

	b2Vec2 m_rayStart;
	b2Vec2 m_rayEnd;

	b2Vec2 m_basePosition;
	float m_baseAngle;

	b2Vec2 m_startPosition;

	bool m_rayDrag;
	bool m_translating;
	bool m_rotating;
	bool m_showFraction;
};

static int sampleIndex = RegisterSample( "Collision", "Ray Cast", RayCast::Create );

// This shows how to filter a specific shape using using data.
struct ShapeUserData
{
	int index;
	bool ignore;
};

// Context for ray cast callbacks. Do what you want with this.
struct CastContext
{
	b2Vec2 points[3];
	b2Vec2 normals[3];
	float fractions[3];
	int count;
};

// This callback finds the closest hit. This is the most common callback used in games.
static float RayCastClosestCallback( b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context )
{
	CastContext* rayContext = (CastContext*)context;

	ShapeUserData* userData = (ShapeUserData*)b2Shape_GetUserData( shapeId );

	// Ignore a specific shape. Also ignore initial overlap.
	if ( ( userData != nullptr && userData->ignore ) || fraction == 0.0f )
	{
		// By returning -1, we instruct the calling code to ignore this shape and
		// continue the ray-cast to the next shape.
		return -1.0f;
	}

	rayContext->points[0] = point;
	rayContext->normals[0] = normal;
	rayContext->fractions[0] = fraction;
	rayContext->count = 1;

	// By returning the current fraction, we instruct the calling code to clip the ray and
	// continue the ray-cast to the next shape. WARNING: do not assume that shapes
	// are reported in order. However, by clipping, we can always get the closest shape.
	return fraction;
}

// This callback finds any hit. For this type of query we are usually just checking for obstruction,
// so the hit data is not relevant.
// NOTE: shape hits are not ordered, so this may not return the closest hit
static float RayCastAnyCallback( b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context )
{
	CastContext* rayContext = (CastContext*)context;

	ShapeUserData* userData = (ShapeUserData*)b2Shape_GetUserData( shapeId );

	// Ignore a specific shape. Also ignore initial overlap.
	if ( ( userData != nullptr && userData->ignore ) || fraction == 0.0f )
	{
		// By returning -1, we instruct the calling code to ignore this shape and
		// continue the ray-cast to the next shape.
		return -1.0f;
	}

	rayContext->points[0] = point;
	rayContext->normals[0] = normal;
	rayContext->fractions[0] = fraction;
	rayContext->count = 1;

	// At this point we have a hit, so we know the ray is obstructed.
	// By returning 0, we instruct the calling code to terminate the ray-cast.
	return 0.0f;
}

// This ray cast collects multiple hits along the ray.
// The shapes are not necessary reported in order, so we might not capture
// the closest shape.
// NOTE: shape hits are not ordered, so this may return hits in any order. This means that
// if you limit the number of results, you may discard the closest hit. You can see this
// behavior in the sample.
static float RayCastMultipleCallback( b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context )
{
	CastContext* rayContext = (CastContext*)context;

	ShapeUserData* userData = (ShapeUserData*)b2Shape_GetUserData( shapeId );

	// Ignore a specific shape. Also ignore initial overlap.
	if ( ( userData != nullptr && userData->ignore ) || fraction == 0.0f )
	{
		// By returning -1, we instruct the calling code to ignore this shape and
		// continue the ray-cast to the next shape.
		return -1.0f;
	}

	int count = rayContext->count;
	assert( count < 3 );

	rayContext->points[count] = point;
	rayContext->normals[count] = normal;
	rayContext->fractions[count] = fraction;
	rayContext->count = count + 1;

	if ( rayContext->count == 3 )
	{
		// At this point the buffer is full.
		// By returning 0, we instruct the calling code to terminate the ray-cast.
		return 0.0f;
	}

	// By returning 1, we instruct the caller to continue without clipping the ray.
	return 1.0f;
}

// This ray cast collects multiple hits along the ray and sorts them.
static float RayCastSortedCallback( b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction, void* context )
{
	CastContext* rayContext = (CastContext*)context;

	ShapeUserData* userData = (ShapeUserData*)b2Shape_GetUserData( shapeId );

	// Ignore a specific shape. Also ignore initial overlap.
	if ( ( userData != nullptr && userData->ignore ) || fraction == 0.0f )
	{
		// By returning -1, we instruct the calling code to ignore this shape and
		// continue the ray-cast to the next shape.
		return -1.0f;
	}

	int count = rayContext->count;
	assert( count <= 3 );

	int index = 3;
	while ( fraction < rayContext->fractions[index - 1] )
	{
		index -= 1;

		if ( index == 0 )
		{
			break;
		}
	}

	if ( index == 3 )
	{
		// not closer, continue but tell the caller not to consider fractions further than the largest fraction acquired
		// this only happens once the buffer is full
		assert( rayContext->count == 3 );
		assert( rayContext->fractions[2] <= 1.0f );
		return rayContext->fractions[2];
	}

	for ( int j = 2; j > index; --j )
	{
		rayContext->points[j] = rayContext->points[j - 1];
		rayContext->normals[j] = rayContext->normals[j - 1];
		rayContext->fractions[j] = rayContext->fractions[j - 1];
	}

	rayContext->points[index] = point;
	rayContext->normals[index] = normal;
	rayContext->fractions[index] = fraction;
	rayContext->count = count < 3 ? count + 1 : 3;

	if ( rayContext->count == 3 )
	{
		return rayContext->fractions[2];
	}

	// By returning 1, we instruct the caller to continue without clipping the ray.
	return 1.0f;
}

// This sample shows how to use the ray and shape cast functions on a b2World. This
// sample is configured to ignore initial overlap.
class CastWorld : public Sample
{
public:
	enum Mode
	{
		e_any = 0,
		e_closest = 1,
		e_multiple = 2,
		e_sorted = 3
	};

	enum CastType
	{
		e_rayCast = 0,
		e_circleCast = 1,
		e_capsuleCast = 2,
		e_polygonCast = 3
	};

	enum
	{
		e_maxCount = 64
	};

	explicit CastWorld( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 2.0f, 14.0f };
			m_context->camera.m_zoom = 25.0f * 0.75f;
		}

		// Ground body
		{
			b2BodyDef bodyDef = b2DefaultBodyDef();
			b2BodyId groundId = b2CreateBody( m_worldId, &bodyDef );

			b2ShapeDef shapeDef = b2DefaultShapeDef();
			b2Segment segment = { { -40.0f, 0.0f }, { 40.0f, 0.0f } };
			b2CreateSegmentShape( groundId, &shapeDef, &segment );
		}

		{
			b2Vec2 vertices[3] = { { -0.1f, 0.0f }, { 0.1f, 0.0f }, { 0.0f, 1.5f } };
			b2Hull hull = b2ComputeHull( vertices, 3 );
			m_polygons[0] = b2MakePolygon( &hull, 0.0f );
			m_polygons[0].radius = 0.5f;
		}

		{
			float w = 1.0f;
			float b = w / ( 2.0f + sqrtf( 2.0f ) );
			float s = sqrtf( 2.0f ) * b;

			b2Vec2 vertices[8] = { { 0.5f * s, 0.0f }, { 0.5f * w, b },		 { 0.5f * w, b + s }, { 0.5f * s, w },
								   { -0.5f * s, w },   { -0.5f * w, b + s }, { -0.5f * w, b },	  { -0.5f * s, 0.0f } };

			b2Hull hull = b2ComputeHull( vertices, 8 );
			m_polygons[1] = b2MakePolygon( &hull, 0.0f );
		}

		m_box = b2MakeBox( 0.5f, 0.5f );
		m_capsule = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, 0.25f };
		m_circle = { { 0.0f, 0.0f }, 0.5f };
		m_segment = { { -1.0f, 0.0f }, { 1.0f, 0.0f } };

		m_bodyIndex = 0;

		for ( int i = 0; i < e_maxCount; ++i )
		{
			m_bodyIds[i] = b2_nullBodyId;
		}

		m_mode = e_closest;
		m_ignoreIndex = 7;

		m_castType = e_rayCast;
		m_castRadius = 0.5f;

		m_rayStart = { -20.0f, 10.0f };
		m_rayEnd = { 20.0f, 10.0f };
		m_dragging = false;

		m_angle = 0.0f;
		m_baseAngle = 0.0f;
		m_angleAnchor = { 0.0f, 0.0f };
		m_rotating = false;

		m_simple = false;
	}

	void Create( int index )
	{
		if ( B2_IS_NON_NULL( m_bodyIds[m_bodyIndex] ) )
		{
			b2DestroyBody( m_bodyIds[m_bodyIndex] );
			m_bodyIds[m_bodyIndex] = b2_nullBodyId;
		}

		float x = RandomFloatRange( -20.0f, 20.0f );
		float y = RandomFloatRange( 0.0f, 20.0f );

		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.position = { x, y };
		bodyDef.rotation = b2MakeRot( RandomFloatRange( -B2_PI, B2_PI ) );

		int mod = m_bodyIndex % 3;
		if ( mod == 0 )
		{
			bodyDef.type = b2_staticBody;
		}
		else if ( mod == 1 )
		{
			bodyDef.type = b2_kinematicBody;
		}
		else if ( mod == 2 )
		{
			bodyDef.type = b2_dynamicBody;
			bodyDef.gravityScale = 0.0f;
		}

		m_bodyIds[m_bodyIndex] = b2CreateBody( m_worldId, &bodyDef );

		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.userData = m_userData + m_bodyIndex;
		m_userData[m_bodyIndex].ignore = false;
		if ( m_bodyIndex == m_ignoreIndex )
		{
			m_userData[m_bodyIndex].ignore = true;
		}

		if ( index == 0 )
		{
			int polygonIndex = ( m_bodyIndex & 1 );
			b2CreatePolygonShape( m_bodyIds[m_bodyIndex], &shapeDef, m_polygons + polygonIndex );
		}
		else if ( index == 1 )
		{
			b2CreatePolygonShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_box );
		}
		else if ( index == 2 )
		{
			b2CreateCircleShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_circle );
		}
		else if ( index == 3 )
		{
			b2CreateCapsuleShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_capsule );
		}
		else if ( index == 4 )
		{
			b2CreateSegmentShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_segment );
		}
		else
		{
			b2Vec2 points[4] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { -1.0f, -1.0f }, { 1.0f, -1.0f } };
			b2ChainDef chainDef = b2DefaultChainDef();
			chainDef.points = points;
			chainDef.count = 4;
			chainDef.isLoop = true;
			b2CreateChain( m_bodyIds[m_bodyIndex], &chainDef );
		}

		m_bodyIndex = ( m_bodyIndex + 1 ) % e_maxCount;
	}

	void CreateN( int index, int count )
	{
		for ( int i = 0; i < count; ++i )
		{
			Create( index );
		}
	}

	void DestroyBody()
	{
		for ( int i = 0; i < e_maxCount; ++i )
		{
			if ( B2_IS_NON_NULL( m_bodyIds[i] ) )
			{
				b2DestroyBody( m_bodyIds[i] );
				m_bodyIds[i] = b2_nullBodyId;
				return;
			}
		}
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 && m_rotating == false )
			{
				m_rayStart = p;
				m_rayEnd = p;
				m_dragging = true;
			}
			else if ( mods == GLFW_MOD_SHIFT && m_dragging == false )
			{
				m_rotating = true;
				m_angleAnchor = p;
				m_baseAngle = m_angle;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_dragging = false;
			m_rotating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_dragging )
		{
			m_rayEnd = p;
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_angleAnchor.x;
			m_angle = m_baseAngle + 1.0f * dx;
		}
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 320.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 200.0f, height ) );

		ImGui::Begin( "Ray-cast World", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		ImGui::Checkbox( "Simple", &m_simple );

		if ( m_simple == false )
		{
			const char* castTypes[] = { "Ray", "Circle", "Capsule", "Polygon" };
			int castType = int( m_castType );
			if ( ImGui::Combo( "Type", &castType, castTypes, IM_ARRAYSIZE( castTypes ) ) )
			{
				m_castType = CastType( castType );
			}

			if ( m_castType != e_rayCast )
			{
				ImGui::SliderFloat( "Radius", &m_castRadius, 0.0f, 2.0f, "%.1f" );
			}

			const char* modes[] = { "Any", "Closest", "Multiple", "Sorted" };
			int mode = int( m_mode );
			if ( ImGui::Combo( "Mode", &mode, modes, IM_ARRAYSIZE( modes ) ) )
			{
				m_mode = Mode( mode );
			}
		}

		if ( ImGui::Button( "Polygon" ) )
			Create( 0 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Poly" ) )
			CreateN( 0, 10 );

		if ( ImGui::Button( "Box" ) )
			Create( 1 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Box" ) )
			CreateN( 1, 10 );

		if ( ImGui::Button( "Circle" ) )
			Create( 2 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Circle" ) )
			CreateN( 2, 10 );

		if ( ImGui::Button( "Capsule" ) )
			Create( 3 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Capsule" ) )
			CreateN( 3, 10 );

		if ( ImGui::Button( "Segment" ) )
			Create( 4 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Segment" ) )
			CreateN( 4, 10 );

		if ( ImGui::Button( "Chain" ) )
			Create( 5 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Chain" ) )
			CreateN( 5, 10 );

		if ( ImGui::Button( "Destroy Shape" ) )
		{
			DestroyBody();
		}

		ImGui::End();
	}

	void Step() override
	{
		Sample::Step();

		DrawTextLine( "Click left mouse button and drag to modify ray cast" );
		DrawTextLine( "Shape 7 is intentionally ignored by the ray" );

		b2HexColor color1 = b2_colorGreen;
		b2HexColor color2 = b2_colorLightGray;
		b2HexColor color3 = b2_colorMagenta;

		b2Vec2 rayTranslation = b2Sub( m_rayEnd, m_rayStart );

		if ( m_simple )
		{
			DrawTextLine( "Simple closest point ray cast" );

			// This version doesn't have a callback, but it doesn't skip the ignored shape
			b2RayResult result = b2World_CastRayClosest( m_worldId, m_rayStart, rayTranslation, b2DefaultQueryFilter() );

			if ( result.hit == true && result.fraction > 0.0f )
			{
				b2Vec2 c = b2MulAdd( m_rayStart, result.fraction, rayTranslation );
				m_context->draw.DrawPoint( result.point, 5.0f, color1 );
				m_context->draw.DrawLine( m_rayStart, c, color2 );
				b2Vec2 head = b2MulAdd( result.point, 0.5f, result.normal );
				m_context->draw.DrawLine( result.point, head, color3 );
			}
			else
			{
				m_context->draw.DrawLine( m_rayStart, m_rayEnd, color2 );
			}
		}
		else
		{
			switch ( m_mode )
			{
				case e_any:
					DrawTextLine( "Cast mode: any - check for obstruction - unsorted" );
					break;

				case e_closest:
					DrawTextLine( "Cast mode: closest - find closest shape along the cast" );
					break;

				case e_multiple:
					DrawTextLine( "Cast mode: multiple - gather up to 3 shapes - unsorted" );
					break;

				case e_sorted:
					DrawTextLine( "Cast mode: sorted - gather up to 3 shapes sorted by closeness" );
					break;

				default:
					assert( false );
					break;
			}

			b2CastResultFcn* functions[] = {
				RayCastAnyCallback,
				RayCastClosestCallback,
				RayCastMultipleCallback,
				RayCastSortedCallback,
			};
			b2CastResultFcn* modeFcn = functions[m_mode];

			CastContext context = {};

			// Must initialize fractions for sorting
			context.fractions[0] = FLT_MAX;
			context.fractions[1] = FLT_MAX;
			context.fractions[2] = FLT_MAX;

			b2Transform transform = { m_rayStart, b2MakeRot( m_angle ) };
			b2Circle circle = { .center = m_rayStart, .radius = m_castRadius };
			b2Capsule capsule = { b2TransformPoint( transform, { -0.25f, 0.0f } ), b2TransformPoint( transform, { 0.25f, 0.0f } ),
								  m_castRadius };
			b2Polygon box = b2MakeOffsetRoundedBox( 0.125f, 0.25f, transform.p, transform.q, m_castRadius );
			b2ShapeProxy proxy = {};

			if ( m_castType == e_rayCast )
			{
				b2World_CastRay( m_worldId, m_rayStart, rayTranslation, b2DefaultQueryFilter(), modeFcn, &context );
			}
			else
			{
				if ( m_castType == e_circleCast )
				{
					proxy = b2MakeProxy( &circle.center, 1, circle.radius );
				}
				else if ( m_castType == e_capsuleCast )
				{
					proxy = b2MakeProxy( &capsule.center1, 2, capsule.radius );
				}
				else
				{
					proxy = b2MakeProxy( box.vertices, box.count, box.radius );
				}

				b2World_CastShape( m_worldId, &proxy, rayTranslation, b2DefaultQueryFilter(), modeFcn, &context );
			}

			if ( context.count > 0 )
			{
				assert( context.count <= 3 );
				b2HexColor colors[3] = { b2_colorRed, b2_colorGreen, b2_colorBlue };
				for ( int i = 0; i < context.count; ++i )
				{
					b2Vec2 c = b2MulAdd( m_rayStart, context.fractions[i], rayTranslation );
					b2Vec2 p = context.points[i];
					b2Vec2 n = context.normals[i];
					m_context->draw.DrawPoint( p, 5.0f, colors[i] );
					m_context->draw.DrawLine( m_rayStart, c, color2 );
					b2Vec2 head = b2MulAdd( p, 1.0f, n );
					m_context->draw.DrawLine( p, head, color3 );

					b2Vec2 t = b2MulSV( context.fractions[i], rayTranslation );
					b2Transform shiftedTransform = { t, b2Rot_identity };

					if ( m_castType == e_circleCast )
					{
						m_context->draw.DrawSolidCircle( shiftedTransform, circle.center, m_castRadius, b2_colorYellow );
					}
					else if ( m_castType == e_capsuleCast )
					{
						b2Vec2 p1 = capsule.center1 + t;
						b2Vec2 p2 = capsule.center2 + t;
						m_context->draw.DrawSolidCapsule( p1, p2, m_castRadius, b2_colorYellow );
					}
					else if ( m_castType == e_polygonCast )
					{
						m_context->draw.DrawSolidPolygon( shiftedTransform, box.vertices, box.count, box.radius, b2_colorYellow );
					}
				}
			}
			else
			{
				m_context->draw.DrawLine( m_rayStart, m_rayEnd, color2 );
				b2Transform shiftedTransform = { rayTranslation, b2Rot_identity };

				if ( m_castType == e_circleCast )
				{
					m_context->draw.DrawSolidCircle( shiftedTransform, circle.center, m_castRadius, b2_colorGray );
				}
				else if ( m_castType == e_capsuleCast )
				{
					b2Vec2 p1 = capsule.center1 + rayTranslation;
					b2Vec2 p2 = capsule.center2 + rayTranslation;
					m_context->draw.DrawSolidCapsule( p1, p2, m_castRadius, b2_colorYellow );
				}
				else if ( m_castType == e_polygonCast )
				{
					m_context->draw.DrawSolidPolygon( shiftedTransform, box.vertices, box.count, box.radius, b2_colorYellow );
				}
			}
		}

		m_context->draw.DrawPoint( m_rayStart, 5.0f, b2_colorGreen );

		if ( B2_IS_NON_NULL( m_bodyIds[m_ignoreIndex] ) )
		{
			b2Vec2 p = b2Body_GetPosition( m_bodyIds[m_ignoreIndex] );
			p.x -= 0.2f;
			m_context->draw.DrawString( p, "ign" );
		}
	}

	static Sample* Create( SampleContext* context )
	{
		return new CastWorld( context );
	}

	int m_bodyIndex;
	b2BodyId m_bodyIds[e_maxCount] = {};
	ShapeUserData m_userData[e_maxCount] = {};
	b2Polygon m_polygons[2] = {};
	b2Polygon m_box;
	b2Capsule m_capsule;
	b2Circle m_circle;
	b2Segment m_segment;

	bool m_simple;

	int m_mode;
	int m_ignoreIndex;

	CastType m_castType;
	float m_castRadius;

	b2Vec2 m_angleAnchor;
	float m_baseAngle;
	float m_angle;
	bool m_rotating;

	b2Vec2 m_rayStart;
	b2Vec2 m_rayEnd;
	bool m_dragging;
};

static int sampleRayCastWorld = RegisterSample( "Collision", "Cast World", CastWorld::Create );

class OverlapWorld : public Sample
{
public:
	enum
	{
		e_circleShape = 0,
		e_capsuleShape = 1,
		e_boxShape = 2
	};

	enum
	{
		e_maxCount = 64,
		e_maxDoomed = 16,
	};

	static bool OverlapResultFcn( b2ShapeId shapeId, void* context )
	{
		ShapeUserData* userData = (ShapeUserData*)b2Shape_GetUserData( shapeId );
		if ( userData != nullptr && userData->ignore )
		{
			// continue the query
			return true;
		}

		OverlapWorld* sample = (OverlapWorld*)context;

		if ( sample->m_doomCount < e_maxDoomed )
		{
			int index = sample->m_doomCount;
			sample->m_doomIds[index] = shapeId;
			sample->m_doomCount += 1;
		}

		// continue the query
		return true;
	}

	explicit OverlapWorld( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 0.0f, 10.0f };
			m_context->camera.m_zoom = 25.0f * 0.7f;
		}

		{
			b2Vec2 vertices[3] = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 1.5f } };
			b2Hull hull = b2ComputeHull( vertices, 3 );
			m_polygons[0] = b2MakePolygon( &hull, 0.0f );
		}

		{
			b2Vec2 vertices[3] = { { -0.1f, 0.0f }, { 0.1f, 0.0f }, { 0.0f, 1.5f } };
			b2Hull hull = b2ComputeHull( vertices, 3 );
			m_polygons[1] = b2MakePolygon( &hull, 0.0f );
		}

		{
			float w = 1.0f;
			float b = w / ( 2.0f + sqrtf( 2.0f ) );
			float s = sqrtf( 2.0f ) * b;

			b2Vec2 vertices[8] = { { 0.5f * s, 0.0f }, { 0.5f * w, b },		 { 0.5f * w, b + s }, { 0.5f * s, w },
								   { -0.5f * s, w },   { -0.5f * w, b + s }, { -0.5f * w, b },	  { -0.5f * s, 0.0f } };

			b2Hull hull = b2ComputeHull( vertices, 8 );
			m_polygons[2] = b2MakePolygon( &hull, 0.0f );
		}

		m_polygons[3] = b2MakeBox( 0.5f, 0.5f );
		m_capsule = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, 0.25f };
		m_circle = { { 0.0f, 0.0f }, 0.5f };
		m_segment = { { -1.0f, 0.0f }, { 1.0f, 0.0f } };

		m_bodyIndex = 0;

		for ( int i = 0; i < e_maxCount; ++i )
		{
			m_bodyIds[i] = b2_nullBodyId;
		}

		m_ignoreIndex = 7;

		m_shapeType = e_circleShape;

		m_position = { 0.0f, 10.0f };
		m_angle = 0.0f;
		m_dragging = false;
		m_rotating = false;

		m_doomCount = 0;

		CreateN( 0, 10 );
	}

	void Create( int index )
	{
		if ( B2_IS_NON_NULL( m_bodyIds[m_bodyIndex] ) )
		{
			b2DestroyBody( m_bodyIds[m_bodyIndex] );
			m_bodyIds[m_bodyIndex] = b2_nullBodyId;
		}

		float x = RandomFloatRange( -20.0f, 20.0f );
		float y = RandomFloatRange( 0.0f, 20.0f );

		b2BodyDef bodyDef = b2DefaultBodyDef();
		bodyDef.position = { x, y };
		bodyDef.rotation = b2MakeRot( RandomFloatRange( -B2_PI, B2_PI ) );

		m_bodyIds[m_bodyIndex] = b2CreateBody( m_worldId, &bodyDef );

		b2ShapeDef shapeDef = b2DefaultShapeDef();
		shapeDef.userData = m_userData + m_bodyIndex;
		m_userData[m_bodyIndex].index = m_bodyIndex;
		m_userData[m_bodyIndex].ignore = false;
		if ( m_bodyIndex == m_ignoreIndex )
		{
			m_userData[m_bodyIndex].ignore = true;
		}

		if ( index < 4 )
		{
			b2CreatePolygonShape( m_bodyIds[m_bodyIndex], &shapeDef, m_polygons + index );
		}
		else if ( index == 4 )
		{
			b2CreateCircleShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_circle );
		}
		else if ( index == 5 )
		{
			b2CreateCapsuleShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_capsule );
		}
		else
		{
			b2CreateSegmentShape( m_bodyIds[m_bodyIndex], &shapeDef, &m_segment );
		}

		m_bodyIndex = ( m_bodyIndex + 1 ) % e_maxCount;
	}

	void CreateN( int index, int count )
	{
		for ( int i = 0; i < count; ++i )
		{
			Create( index );
		}
	}

	void DestroyBody()
	{
		for ( int i = 0; i < e_maxCount; ++i )
		{
			if ( B2_IS_NON_NULL( m_bodyIds[i] ) )
			{
				b2DestroyBody( m_bodyIds[i] );
				m_bodyIds[i] = b2_nullBodyId;
				return;
			}
		}
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 && m_rotating == false )
			{
				m_dragging = true;
				m_position = p;
			}
			else if ( mods == GLFW_MOD_SHIFT && m_dragging == false )
			{
				m_rotating = true;
				m_startPosition = p;
				m_baseAngle = m_angle;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_dragging = false;
			m_rotating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_dragging )
		{
			m_position = p;
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_startPosition.x;
			m_angle = m_baseAngle + 1.0f * dx;
		}
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 330.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 140.0f, height ) );

		ImGui::Begin( "Overlap World", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		if ( ImGui::Button( "Polygon 1" ) )
			Create( 0 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Poly1" ) )
			CreateN( 0, 10 );

		if ( ImGui::Button( "Polygon 2" ) )
			Create( 1 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Poly2" ) )
			CreateN( 1, 10 );

		if ( ImGui::Button( "Polygon 3" ) )
			Create( 2 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Poly3" ) )
			CreateN( 2, 10 );

		if ( ImGui::Button( "Box" ) )
			Create( 3 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Box" ) )
			CreateN( 3, 10 );

		if ( ImGui::Button( "Circle" ) )
			Create( 4 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Circle" ) )
			CreateN( 4, 10 );

		if ( ImGui::Button( "Capsule" ) )
			Create( 5 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Capsule" ) )
			CreateN( 5, 10 );

		if ( ImGui::Button( "Segment" ) )
			Create( 6 );
		ImGui::SameLine();
		if ( ImGui::Button( "10x##Segment" ) )
			CreateN( 6, 10 );

		if ( ImGui::Button( "Destroy Shape" ) )
		{
			DestroyBody();
		}

		ImGui::Separator();
		ImGui::Text( "Overlap Shape" );
		ImGui::RadioButton( "Circle##Overlap", &m_shapeType, e_circleShape );
		ImGui::RadioButton( "Capsule##Overlap", &m_shapeType, e_capsuleShape );
		ImGui::RadioButton( "Box##Overlap", &m_shapeType, e_boxShape );

		ImGui::End();
	}

	void Step() override
	{
		Sample::Step();

		DrawTextLine( "left mouse button: drag query shape" );
		DrawTextLine( "left mouse button + shift: rotate query shape" );

		m_doomCount = 0;

		b2Transform transform = { m_position, b2MakeRot( m_angle ) };
		b2ShapeProxy proxy = {};

		if ( m_shapeType == e_circleShape )
		{
			b2Circle circle = {
				.center = transform.p,
				.radius = 1.0f,
			};
			proxy = b2MakeProxy( &circle.center, 1, circle.radius );
			m_context->draw.DrawSolidCircle( b2Transform_identity, circle.center, circle.radius, b2_colorWhite );
		}
		else if ( m_shapeType == e_capsuleShape )
		{
			b2Capsule capsule = {
				.center1 = b2TransformPoint( transform, { -1.0f, 0.0f } ),
				.center2 = b2TransformPoint( transform, { 1.0f, 0.0f } ),
				.radius = 0.5f,
			};
			proxy = b2MakeProxy( &capsule.center1, 2, capsule.radius );
			m_context->draw.DrawSolidCapsule( capsule.center1, capsule.center2, capsule.radius, b2_colorWhite );
		}
		else if ( m_shapeType == e_boxShape )
		{
			b2Polygon box = b2MakeOffsetBox( 2.0f, 0.5f, transform.p, transform.q );
			proxy = b2MakeProxy( box.vertices, box.count, box.radius );
			m_context->draw.DrawPolygon( box.vertices, box.count, b2_colorWhite );
		}

		b2World_OverlapShape( m_worldId, &proxy, b2DefaultQueryFilter(), OverlapResultFcn, this );

		if ( B2_IS_NON_NULL( m_bodyIds[m_ignoreIndex] ) )
		{
			b2Vec2 p = b2Body_GetPosition( m_bodyIds[m_ignoreIndex] );
			p.x -= 0.2f;
			m_context->draw.DrawString( p, "skip" );
		}

		for ( int i = 0; i < m_doomCount; ++i )
		{
			b2ShapeId shapeId = m_doomIds[i];
			ShapeUserData* userData = (ShapeUserData*)b2Shape_GetUserData( shapeId );
			if ( userData == nullptr )
			{
				continue;
			}

			int index = userData->index;
			assert( 0 <= index && index < e_maxCount );
			assert( B2_IS_NON_NULL( m_bodyIds[index] ) );

			b2DestroyBody( m_bodyIds[index] );
			m_bodyIds[index] = b2_nullBodyId;
		}
	}

	static Sample* Create( SampleContext* context )
	{
		return new OverlapWorld( context );
	}

	int m_bodyIndex;
	b2BodyId m_bodyIds[e_maxCount];
	ShapeUserData m_userData[e_maxCount];
	b2Polygon m_polygons[4];
	b2Capsule m_capsule;
	b2Circle m_circle;
	b2Segment m_segment;
	int m_ignoreIndex;

	b2ShapeId m_doomIds[e_maxDoomed];
	int m_doomCount;

	int m_shapeType;
	b2Transform m_transform;

	b2Vec2 m_startPosition;

	b2Vec2 m_position;
	b2Vec2 m_basePosition;
	float m_angle;
	float m_baseAngle;

	bool m_dragging;
	bool m_rotating;
};

static int sampleOverlapWorld = RegisterSample( "Collision", "Overlap World", OverlapWorld::Create );

// Tests manifolds and contact points
class Manifold : public Sample
{
public:
	explicit Manifold( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			// m_context->camera.m_center = {1.8f, 15.0f};
			m_context->camera.m_center = { 1.8f, 0.0f };
			m_context->camera.m_zoom = 25.0f * 0.45f;
		}

		m_smgroxCache1 = b2_emptySimplexCache;
		m_smgroxCache2 = b2_emptySimplexCache;
		m_smgcapCache1 = b2_emptySimplexCache;
		m_smgcapCache2 = b2_emptySimplexCache;

		m_transform = b2Transform_identity;
		m_transform.p.x = 0.17f;
		m_transform.p.y = 1.12f;
		// m_transform.q = b2MakeRot( 0.5f * b2_pi );
		m_angle = 0.0f;
		m_round = 0.1f;

		m_startPoint = { 0.0f, 0.0f };
		m_basePosition = { 0.0f, 0.0f };
		m_baseAngle = 0.0f;

		m_dragging = false;
		m_rotating = false;
		m_showCount = false;
		m_showIds = false;
		m_showSeparation = false;
		m_showAnchors = false;
		m_enableCaching = true;

		b2Vec2 points[3] = { { -0.1f, -0.5f }, { 0.1f, -0.5f }, { 0.0f, 0.5f } };
		m_wedge = b2ComputeHull( points, 3 );
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 24.0f * fontSize;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 20.0f * fontSize, height ) );

		ImGui::Begin( "Manifold", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		ImGui::PushItemWidth( 14.0f * fontSize );

		ImGui::SliderFloat( "x offset", &m_transform.p.x, -2.0f, 2.0f, "%.2f" );
		ImGui::SliderFloat( "y offset", &m_transform.p.y, -2.0f, 2.0f, "%.2f" );

		if ( ImGui::SliderFloat( "angle", &m_angle, -B2_PI, B2_PI, "%.2f" ) )
		{
			m_transform.q = b2MakeRot( m_angle );
		}

		ImGui::SliderFloat( "round", &m_round, 0.0f, 0.4f, "%.1f" );

		ImGui::PopItemWidth();

		ImGui::Separator();

		ImGui::Checkbox( "show count", &m_showCount );
		ImGui::Checkbox( "show ids", &m_showIds );
		ImGui::Checkbox( "show separation", &m_showSeparation );
		ImGui::Checkbox( "show anchors", &m_showAnchors );
		ImGui::Checkbox( "enable caching", &m_enableCaching );

		if ( ImGui::Button( "Reset" ) )
		{
			m_transform = b2Transform_identity;
			m_angle = 0.0f;
		}

		ImGui::Separator();

		ImGui::Text( "mouse button 1: drag" );
		ImGui::Text( "mouse button 1 + shift: rotate" );

		ImGui::End();
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 && m_rotating == false )
			{
				m_dragging = true;
				m_startPoint = p;
				m_basePosition = m_transform.p;
			}
			else if ( mods == GLFW_MOD_SHIFT && m_dragging == false )
			{
				m_rotating = true;
				m_startPoint = p;
				m_baseAngle = m_angle;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_dragging = false;
			m_rotating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_dragging )
		{
			m_transform.p.x = m_basePosition.x + 0.5f * ( p.x - m_startPoint.x );
			m_transform.p.y = m_basePosition.y + 0.5f * ( p.y - m_startPoint.y );
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_startPoint.x;
			m_angle = b2ClampFloat( m_baseAngle + 1.0f * dx, -B2_PI, B2_PI );
			m_transform.q = b2MakeRot( m_angle );
		}
	}

	void DrawManifold( const b2Manifold* manifold, b2Vec2 origin1, b2Vec2 origin2 )
	{
		if ( m_showCount )
		{
			b2Vec2 p = 0.5f * ( origin1 + origin2 );
			m_context->draw.DrawString( p, "%d", manifold->pointCount );
		}

		for ( int i = 0; i < manifold->pointCount; ++i )
		{
			const b2ManifoldPoint* mp = manifold->points + i;

			b2Vec2 p1 = mp->point;
			b2Vec2 p2 = b2MulAdd( p1, 0.5f, manifold->normal );
			m_context->draw.DrawLine( p1, p2, b2_colorViolet );

			if ( m_showAnchors )
			{
				m_context->draw.DrawPoint( b2Add( origin1, mp->anchorA ), 5.0f, b2_colorRed );
				m_context->draw.DrawPoint( b2Add( origin2, mp->anchorB ), 5.0f, b2_colorGreen );
			}
			else
			{
				m_context->draw.DrawPoint( p1, 10.0f, b2_colorBlue );
			}

			if ( m_showIds )
			{
				// uint32_t indexA = mp->id >> 8;
				// uint32_t indexB = 0xFF & mp->id;
				b2Vec2 p = { p1.x + 0.05f, p1.y - 0.02f };
				m_context->draw.DrawString( p, "0x%04x", mp->id );
			}

			if ( m_showSeparation )
			{
				b2Vec2 p = { p1.x + 0.05f, p1.y + 0.03f };
				m_context->draw.DrawString( p, "%.3f", mp->separation );
			}
		}
	}

	void Step() override
	{
		b2Vec2 offset = { -10.0f, -5.0f };
		b2Vec2 increment = { 4.0f, 0.0f };

		b2HexColor color1 = b2_colorAquamarine;
		b2HexColor color2 = b2_colorPaleGoldenRod;

		if ( m_enableCaching == false )
		{
			m_smgroxCache1 = b2_emptySimplexCache;
			m_smgroxCache2 = b2_emptySimplexCache;
			m_smgcapCache1 = b2_emptySimplexCache;
			m_smgcapCache2 = b2_emptySimplexCache;
		}

#if 1
		// circle-circle
		{
			b2Circle circle1 = { { 0.0f, 0.0f }, 0.5f };
			b2Circle circle2 = { { 0.0f, 0.0f }, 1.0f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollideCircles( &circle1, transform1, &circle2, transform2 );

			m_context->draw.DrawSolidCircle( transform1, circle1.center, circle1.radius, color1 );
			m_context->draw.DrawSolidCircle( transform2, circle2.center, circle2.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// capsule-circle
		{
			b2Capsule capsule = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, 0.25f };
			b2Circle circle = { { 0.0f, 0.0f }, 0.5f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollideCapsuleAndCircle( &capsule, transform1, &circle, transform2 );

			b2Vec2 v1 = b2TransformPoint( transform1, capsule.center1 );
			b2Vec2 v2 = b2TransformPoint( transform1, capsule.center2 );
			m_context->draw.DrawSolidCapsule( v1, v2, capsule.radius, color1 );

			m_context->draw.DrawSolidCircle( transform2, circle.center, circle.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// segment-circle
		{
			b2Segment segment = { { -1.0f, 0.0f }, { 1.0f, 0.0f } };
			b2Circle circle = { { 0.0f, 0.0f }, 0.5f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollideSegmentAndCircle( &segment, transform1, &circle, transform2 );

			b2Vec2 p1 = b2TransformPoint( transform1, segment.point1 );
			b2Vec2 p2 = b2TransformPoint( transform1, segment.point2 );
			m_context->draw.DrawLine( p1, p2, color1 );

			m_context->draw.DrawSolidCircle( transform2, circle.center, circle.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// box-circle
		{
			b2Circle circle = { { 0.0f, 0.0f }, 0.5f };
			b2Polygon box = b2MakeSquare( 0.5f );
			box.radius = m_round;

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollidePolygonAndCircle( &box, transform1, &circle, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, box.vertices, box.count, m_round, color1 );
			m_context->draw.DrawSolidCircle( transform2, circle.center, circle.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// capsule-capsule
		{
			b2Capsule capsule1 = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, 0.25f };
			b2Capsule capsule2 = { { 0.25f, 0.0f }, { 1.0f, 0.0f }, 0.1f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollideCapsules( &capsule1, transform1, &capsule2, transform2 );

			b2Vec2 v1 = b2TransformPoint( transform1, capsule1.center1 );
			b2Vec2 v2 = b2TransformPoint( transform1, capsule1.center2 );
			m_context->draw.DrawSolidCapsule( v1, v2, capsule1.radius, color1 );

			v1 = b2TransformPoint( transform2, capsule2.center1 );
			v2 = b2TransformPoint( transform2, capsule2.center2 );
			m_context->draw.DrawSolidCapsule( v1, v2, capsule2.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// box-capsule
		{
			b2Capsule capsule = { { -0.4f, 0.0f }, { -0.1f, 0.0f }, 0.1f };
			b2Polygon box = b2MakeOffsetBox( 0.25f, 1.0f, { 1.0f, -1.0f }, b2MakeRot( 0.25f * B2_PI ) );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollidePolygonAndCapsule( &box, transform1, &capsule, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, box.vertices, box.count, box.radius, color1 );

			b2Vec2 v1 = b2TransformPoint( transform2, capsule.center1 );
			b2Vec2 v2 = b2TransformPoint( transform2, capsule.center2 );
			m_context->draw.DrawSolidCapsule( v1, v2, capsule.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// segment-capsule
		{
			b2Segment segment = { { -1.0f, 0.0f }, { 1.0f, 0.0f } };
			b2Capsule capsule = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, 0.25f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollideSegmentAndCapsule( &segment, transform1, &capsule, transform2 );

			b2Vec2 p1 = b2TransformPoint( transform1, segment.point1 );
			b2Vec2 p2 = b2TransformPoint( transform1, segment.point2 );
			m_context->draw.DrawLine( p1, p2, color1 );

			p1 = b2TransformPoint( transform2, capsule.center1 );
			p2 = b2TransformPoint( transform2, capsule.center2 );
			m_context->draw.DrawSolidCapsule( p1, p2, capsule.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		offset = { -10.0f, 0.0f };
#endif

#if 1
		// square-square
		{
			b2Polygon box1 = b2MakeSquare( 0.5f );
			b2Polygon box = b2MakeSquare( 0.5f );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollidePolygons( &box1, transform1, &box, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, box1.vertices, box1.count, box1.radius, color1 );
			m_context->draw.DrawSolidPolygon( transform2, box.vertices, box.count, box.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// box-box
		{
			b2Polygon box1 = b2MakeBox( 2.0f, 0.1f );
			b2Polygon box = b2MakeSquare( 0.25f );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform2 = {b2Add({0.0f, -0.1f}, offset), {0.0f, 1.0f}};

			b2Manifold m = b2CollidePolygons( &box1, transform1, &box, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, box1.vertices, box1.count, box1.radius, color1 );
			m_context->draw.DrawSolidPolygon( transform2, box.vertices, box.count, box.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// box-rox
		{
			b2Polygon box = b2MakeSquare( 0.5f );
			float h = 0.5f - m_round;
			b2Polygon rox = b2MakeRoundedBox( h, h, m_round );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform2 = {b2Add({0.0f, -0.1f}, offset), {0.0f, 1.0f}};

			b2Manifold m = b2CollidePolygons( &box, transform1, &rox, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, box.vertices, box.count, box.radius, color1 );
			m_context->draw.DrawSolidPolygon( transform2, rox.vertices, rox.count, rox.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// rox-rox
		{
			float h = 0.5f - m_round;
			b2Polygon rox = b2MakeRoundedBox( h, h, m_round );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform1 = {{6.48024225f, 2.07872653f}, {-0.938356698f, 0.345668465f}};
			// b2Transform transform2 = {{5.52862263f, 2.51146317f}, {-0.859374702f, -0.511346340f}};

			b2Manifold m = b2CollidePolygons( &rox, transform1, &rox, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, rox.vertices, rox.count, rox.radius, color1 );
			m_context->draw.DrawSolidPolygon( transform2, rox.vertices, rox.count, rox.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// segment-rox
		{
			b2Segment segment = { { -1.0f, 0.0f }, { 1.0f, 0.0f } };
			float h = 0.5f - m_round;
			b2Polygon rox = b2MakeRoundedBox( h, h, m_round );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform2 = {b2Add({-1.44583416f, 0.397352695f}, offset), m_transform.q};

			b2Manifold m = b2CollideSegmentAndPolygon( &segment, transform1, &rox, transform2 );

			b2Vec2 p1 = b2TransformPoint( transform1, segment.point1 );
			b2Vec2 p2 = b2TransformPoint( transform1, segment.point2 );
			m_context->draw.DrawLine( p1, p2, color1 );
			m_context->draw.DrawSolidPolygon( transform2, rox.vertices, rox.count, rox.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}
#endif

		// wox-wox
		{
			b2Polygon wox = b2MakePolygon( &m_wedge, m_round );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform2 = {b2Add({0.0f, -0.1f}, offset), {0.0f, 1.0f}};

			b2Manifold m = b2CollidePolygons( &wox, transform1, &wox, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, wox.vertices, wox.count, wox.radius, color1 );
			m_context->draw.DrawSolidPolygon( transform1, wox.vertices, wox.count, 0.0f, color1 );
			m_context->draw.DrawSolidPolygon( transform2, wox.vertices, wox.count, wox.radius, color2 );
			m_context->draw.DrawSolidPolygon( transform2, wox.vertices, wox.count, 0.0f, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

#if 1
		// wox-wox
		{
			b2Vec2 p1s[3] = { { 0.175740838, 0.224936664 }, { -0.301293969, 0.194021404 }, { -0.105151534, -0.432157338 } };
			b2Vec2 p2s[3] = { { -0.427884758, -0.225028217 }, { 0.0566576123, -0.128772855 }, { 0.176625848, 0.338923335 } };

			b2Hull h1 = b2ComputeHull( p1s, 3 );
			b2Hull h2 = b2ComputeHull( p2s, 3 );
			b2Polygon w1 = b2MakePolygon( &h1, 0.158798501 );
			b2Polygon w2 = b2MakePolygon( &h2, 0.205900759 );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform2 = {b2Add({0.0f, -0.1f}, offset), {0.0f, 1.0f}};

			b2Manifold m = b2CollidePolygons( &w1, transform1, &w2, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, w1.vertices, w1.count, w1.radius, color1 );
			m_context->draw.DrawSolidPolygon( transform1, w1.vertices, w1.count, 0.0f, color1 );
			m_context->draw.DrawSolidPolygon( transform2, w2.vertices, w2.count, w2.radius, color2 );
			m_context->draw.DrawSolidPolygon( transform2, w2.vertices, w2.count, 0.0f, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		offset = { -10.0f, 5.0f };

		// box-triangle
		{
			b2Polygon box = b2MakeBox( 1.0f, 1.0f );
			b2Vec2 points[3] = { { -0.05f, 0.0f }, { 0.05f, 0.0f }, { 0.0f, 0.1f } };
			b2Hull hull = b2ComputeHull( points, 3 );
			b2Polygon tri = b2MakePolygon( &hull, 0.0f );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };
			// b2Transform transform2 = {b2Add({0.0f, -0.1f}, offset), {0.0f, 1.0f}};

			b2Manifold m = b2CollidePolygons( &box, transform1, &tri, transform2 );

			m_context->draw.DrawSolidPolygon( transform1, box.vertices, box.count, 0.0f, color1 );
			m_context->draw.DrawSolidPolygon( transform2, tri.vertices, tri.count, 0.0f, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset = b2Add( offset, increment );
		}

		// chain-segment vs circle
		{
			b2ChainSegment segment = { { 2.0f, 1.0f }, { { 1.0f, 1.0f }, { -1.0f, 0.0f } }, { -2.0f, 0.0f }, -1 };
			b2Circle circle = { { 0.0f, 0.0f }, 0.5f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m = b2CollideChainSegmentAndCircle( &segment, transform1, &circle, transform2 );

			b2Vec2 g1 = b2TransformPoint( transform1, segment.ghost1 );
			b2Vec2 g2 = b2TransformPoint( transform1, segment.ghost2 );
			b2Vec2 p1 = b2TransformPoint( transform1, segment.segment.point1 );
			b2Vec2 p2 = b2TransformPoint( transform1, segment.segment.point2 );
			m_context->draw.DrawLine( g1, p1, b2_colorLightGray );
			m_context->draw.DrawLine( p1, p2, color1 );
			m_context->draw.DrawLine( p2, g2, b2_colorLightGray );
			m_context->draw.DrawSolidCircle( transform2, circle.center, circle.radius, color2 );

			DrawManifold( &m, transform1.p, transform2.p );

			offset.x += 2.0f * increment.x;
		}

		// chain-segment vs rounded polygon
		{
			b2ChainSegment segment1 = { { 2.0f, 1.0f }, { { 1.0f, 1.0f }, { -1.0f, 0.0f } }, { -2.0f, 0.0f }, -1 };
			b2ChainSegment segment2 = { { 3.0f, 1.0f }, { { 2.0f, 1.0f }, { 1.0f, 1.0f } }, { -1.0f, 0.0f }, -1 };
			// b2ChainSegment segment1 = {{2.0f, 0.0f}, {{1.0f, 0.0f}, {-1.0f, 0.0f}}, {-2.0f, 0.0f}, -1};
			// b2ChainSegment segment2 = {{3.0f, 0.0f}, {{2.0f, 0.0f}, {1.0f, 0.0f}}, {-1.0f, 0.0f}, -1};
			// b2ChainSegment segment1 = {{0.5f, 1.0f}, {{0.0f, 2.0f}, {-0.5f, 1.0f}}, {-1.0f, 0.0f}, -1};
			// b2ChainSegment segment2 = {{1.0f, 0.0f}, {{0.5f, 1.0f}, {0.0f, 2.0f}}, {-0.5f, 1.0f}, -1};
			float h = 0.5f - m_round;
			b2Polygon rox = b2MakeRoundedBox( h, h, m_round );

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m1 = b2CollideChainSegmentAndPolygon( &segment1, transform1, &rox, transform2, &m_smgroxCache1 );
			b2Manifold m2 = b2CollideChainSegmentAndPolygon( &segment2, transform1, &rox, transform2, &m_smgroxCache2 );

			{
				b2Vec2 g2 = b2TransformPoint( transform1, segment1.ghost2 );
				b2Vec2 p1 = b2TransformPoint( transform1, segment1.segment.point1 );
				b2Vec2 p2 = b2TransformPoint( transform1, segment1.segment.point2 );
				m_context->draw.DrawLine( p1, p2, color1 );
				m_context->draw.DrawPoint( p1, 4.0f, color1 );
				m_context->draw.DrawPoint( p2, 4.0f, color1 );
				m_context->draw.DrawLine( p2, g2, b2_colorLightGray );
			}

			{
				b2Vec2 g1 = b2TransformPoint( transform1, segment2.ghost1 );
				b2Vec2 p1 = b2TransformPoint( transform1, segment2.segment.point1 );
				b2Vec2 p2 = b2TransformPoint( transform1, segment2.segment.point2 );
				m_context->draw.DrawLine( g1, p1, b2_colorLightGray );
				m_context->draw.DrawLine( p1, p2, color1 );
				m_context->draw.DrawPoint( p1, 4.0f, color1 );
				m_context->draw.DrawPoint( p2, 4.0f, color1 );
			}

			m_context->draw.DrawSolidPolygon( transform2, rox.vertices, rox.count, rox.radius, color2 );
			m_context->draw.DrawPoint( b2TransformPoint( transform2, rox.centroid ), 5.0f, b2_colorGainsboro );

			DrawManifold( &m1, transform1.p, transform2.p );
			DrawManifold( &m2, transform1.p, transform2.p );

			offset.x += 2.0f * increment.x;
		}

		// chain-segment vs capsule
		{
			b2ChainSegment segment1 = { { 2.0f, 1.0f }, { { 1.0f, 1.0f }, { -1.0f, 0.0f } }, { -2.0f, 0.0f }, -1 };
			b2ChainSegment segment2 = { { 3.0f, 1.0f }, { { 2.0f, 1.0f }, { 1.0f, 1.0f } }, { -1.0f, 0.0f }, -1 };
			b2Capsule capsule = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, 0.25f };

			b2Transform transform1 = { offset, b2Rot_identity };
			b2Transform transform2 = { b2Add( m_transform.p, offset ), m_transform.q };

			b2Manifold m1 = b2CollideChainSegmentAndCapsule( &segment1, transform1, &capsule, transform2, &m_smgcapCache1 );
			b2Manifold m2 = b2CollideChainSegmentAndCapsule( &segment2, transform1, &capsule, transform2, &m_smgcapCache2 );

			{
				b2Vec2 g2 = b2TransformPoint( transform1, segment1.ghost2 );
				b2Vec2 p1 = b2TransformPoint( transform1, segment1.segment.point1 );
				b2Vec2 p2 = b2TransformPoint( transform1, segment1.segment.point2 );
				// m_context->draw.DrawSegment(g1, p1, b2_colorLightGray);
				m_context->draw.DrawLine( p1, p2, color1 );
				m_context->draw.DrawPoint( p1, 4.0f, color1 );
				m_context->draw.DrawPoint( p2, 4.0f, color1 );
				m_context->draw.DrawLine( p2, g2, b2_colorLightGray );
			}

			{
				b2Vec2 g1 = b2TransformPoint( transform1, segment2.ghost1 );
				b2Vec2 p1 = b2TransformPoint( transform1, segment2.segment.point1 );
				b2Vec2 p2 = b2TransformPoint( transform1, segment2.segment.point2 );
				m_context->draw.DrawLine( g1, p1, b2_colorLightGray );
				m_context->draw.DrawLine( p1, p2, color1 );
				m_context->draw.DrawPoint( p1, 4.0f, color1 );
				m_context->draw.DrawPoint( p2, 4.0f, color1 );
				// m_context->draw.DrawSegment(p2, g2, b2_colorLightGray);
			}

			b2Vec2 p1 = b2TransformPoint( transform2, capsule.center1 );
			b2Vec2 p2 = b2TransformPoint( transform2, capsule.center2 );
			m_context->draw.DrawSolidCapsule( p1, p2, capsule.radius, color2 );

			m_context->draw.DrawPoint( b2Lerp( p1, p2, 0.5f ), 5.0f, b2_colorGainsboro );

			DrawManifold( &m1, transform1.p, transform2.p );
			DrawManifold( &m2, transform1.p, transform2.p );

			offset.x += 2.0f * increment.x;
		}
#endif
	}

	static Sample* Create( SampleContext* context )
	{
		return new Manifold( context );
	}

	b2SimplexCache m_smgroxCache1;
	b2SimplexCache m_smgroxCache2;
	b2SimplexCache m_smgcapCache1;
	b2SimplexCache m_smgcapCache2;

	b2Hull m_wedge;

	b2Transform m_transform;
	float m_angle;
	float m_round;

	b2Vec2 m_basePosition;
	b2Vec2 m_startPoint;
	float m_baseAngle;

	bool m_dragging;
	bool m_rotating;
	bool m_showCount;
	bool m_showIds;
	bool m_showAnchors;
	bool m_showSeparation;
	bool m_enableCaching;
};

static int sampleManifoldIndex = RegisterSample( "Collision", "Manifold", Manifold::Create );

class SmoothManifold : public Sample
{
public:
	enum ShapeType
	{
		e_circleShape = 0,
		e_boxShape
	};

	explicit SmoothManifold( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 2.0f, 20.0f };
			m_context->camera.m_zoom = 21.0f;
		}

		m_shapeType = e_boxShape;
		m_transform = { { 0.0f, 20.0f }, b2Rot_identity };
		m_angle = 0.0f;
		m_round = 0.0f;

		m_startPoint = { 0.0f, 00.0f };
		m_basePosition = { 0.0f, 0.0f };
		m_baseAngle = 0.0f;

		m_dragging = false;
		m_rotating = false;
		m_showIds = false;
		m_showAnchors = false;
		m_showSeparation = false;

		// https://betravis.github.io/shape-tools/path-to-polygon/
		m_count = 36;

		b2Vec2 points[36];
		points[0] = { -20.58325, 14.54175 };
		points[1] = { -21.90625, 15.8645 };
		points[2] = { -24.552, 17.1875 };
		points[3] = { -27.198, 11.89575 };
		points[4] = { -29.84375, 15.8645 };
		points[5] = { -29.84375, 21.15625 };
		points[6] = { -25.875, 23.802 };
		points[7] = { -20.58325, 25.125 };
		points[8] = { -25.875, 29.09375 };
		points[9] = { -20.58325, 31.7395 };
		points[10] = { -11.0089998, 23.2290001 };
		points[11] = { -8.67700005, 21.15625 };
		points[12] = { -6.03125, 21.15625 };
		points[13] = { -7.35424995, 29.09375 };
		points[14] = { -3.38549995, 29.09375 };
		points[15] = { 1.90625, 30.41675 };
		points[16] = { 5.875, 17.1875 };
		points[17] = { 11.16675, 25.125 };
		points[18] = { 9.84375, 29.09375 };
		points[19] = { 13.8125, 31.7395 };
		points[20] = { 21.75, 30.41675 };
		points[21] = { 28.3644981, 26.448 };
		points[22] = { 25.71875, 18.5105 };
		points[23] = { 24.3957481, 13.21875 };
		points[24] = { 17.78125, 11.89575 };
		points[25] = { 15.1355, 7.92700005 };
		points[26] = { 5.875, 9.25 };
		points[27] = { 1.90625, 11.89575 };
		points[28] = { -3.25, 11.89575 };
		points[29] = { -3.25, 9.9375 };
		points[30] = { -4.70825005, 9.25 };
		points[31] = { -8.67700005, 9.25 };
		points[32] = { -11.323, 11.89575 };
		points[33] = { -13.96875, 11.89575 };
		points[34] = { -15.29175, 14.54175 };
		points[35] = { -19.2605, 14.54175 };

		m_segments = (b2ChainSegment*)malloc( m_count * sizeof( b2ChainSegment ) );

		for ( int i = 0; i < m_count; ++i )
		{
			int i0 = i > 0 ? i - 1 : m_count - 1;
			int i1 = i;
			int i2 = i1 < m_count - 1 ? i1 + 1 : 0;
			int i3 = i2 < m_count - 1 ? i2 + 1 : 0;

			b2Vec2 g1 = points[i0];
			b2Vec2 p1 = points[i1];
			b2Vec2 p2 = points[i2];
			b2Vec2 g2 = points[i3];

			m_segments[i] = { g1, { p1, p2 }, g2, -1 };
		}
	}

	virtual ~SmoothManifold() override
	{
		free( m_segments );
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 290.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 180.0f, height ) );

		ImGui::Begin( "Smooth Manifold", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );
		ImGui::PushItemWidth( 100.0f );

		{
			const char* shapeTypes[] = { "Circle", "Box" };
			int shapeType = int( m_shapeType );
			ImGui::Combo( "Shape", &shapeType, shapeTypes, IM_ARRAYSIZE( shapeTypes ) );
			m_shapeType = ShapeType( shapeType );
		}

		ImGui::SliderFloat( "x Offset", &m_transform.p.x, -2.0f, 2.0f, "%.2f" );
		ImGui::SliderFloat( "y Offset", &m_transform.p.y, -2.0f, 2.0f, "%.2f" );

		if ( ImGui::SliderFloat( "Angle", &m_angle, -B2_PI, B2_PI, "%.2f" ) )
		{
			m_transform.q = b2MakeRot( m_angle );
		}

		ImGui::SliderFloat( "Round", &m_round, 0.0f, 0.4f, "%.1f" );
		ImGui::Checkbox( "Show Ids", &m_showIds );
		ImGui::Checkbox( "Show Separation", &m_showSeparation );
		ImGui::Checkbox( "Show Anchors", &m_showAnchors );

		if ( ImGui::Button( "Reset" ) )
		{
			m_transform = b2Transform_identity;
			m_angle = 0.0f;
		}

		ImGui::Separator();

		ImGui::Text( "mouse button 1: drag" );
		ImGui::Text( "mouse button 1 + shift: rotate" );

		ImGui::PopItemWidth();
		ImGui::End();
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 && m_rotating == false )
			{
				m_dragging = true;
				m_startPoint = p;
				m_basePosition = m_transform.p;
			}
			else if ( mods == GLFW_MOD_SHIFT && m_dragging == false )
			{
				m_rotating = true;
				m_startPoint = p;
				m_baseAngle = m_angle;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_dragging = false;
			m_rotating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_dragging )
		{
			m_transform.p.x = m_basePosition.x + ( p.x - m_startPoint.x );
			m_transform.p.y = m_basePosition.y + ( p.y - m_startPoint.y );
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_startPoint.x;
			m_angle = b2ClampFloat( m_baseAngle + 1.0f * dx, -B2_PI, B2_PI );
			m_transform.q = b2MakeRot( m_angle );
		}
	}

	void DrawManifold( const b2Manifold* manifold )
	{
		for ( int i = 0; i < manifold->pointCount; ++i )
		{
			const b2ManifoldPoint* mp = manifold->points + i;

			b2Vec2 p1 = mp->point;
			b2Vec2 p2 = b2MulAdd( p1, 0.5f, manifold->normal );
			m_context->draw.DrawLine( p1, p2, b2_colorWhite );

			if ( m_showAnchors )
			{
				m_context->draw.DrawPoint( p1, 5.0f, b2_colorGreen );
			}
			else
			{
				m_context->draw.DrawPoint( p1, 5.0f, b2_colorGreen );
			}

			if ( m_showIds )
			{
				// uint32_t indexA = mp->id >> 8;
				// uint32_t indexB = 0xFF & mp->id;
				b2Vec2 p = { p1.x + 0.05f, p1.y - 0.02f };
				m_context->draw.DrawString( p, "0x%04x", mp->id );
			}

			if ( m_showSeparation )
			{
				b2Vec2 p = { p1.x + 0.05f, p1.y + 0.03f };
				m_context->draw.DrawString( p, "%.3f", mp->separation );
			}
		}
	}

	void Step() override
	{
		b2HexColor color1 = b2_colorYellow;
		b2HexColor color2 = b2_colorMagenta;

		b2Transform transform1 = b2Transform_identity;
		b2Transform transform2 = m_transform;

		for ( int i = 0; i < m_count; ++i )
		{
			const b2ChainSegment* segment = m_segments + i;
			b2Vec2 p1 = b2TransformPoint( transform1, segment->segment.point1 );
			b2Vec2 p2 = b2TransformPoint( transform1, segment->segment.point2 );
			m_context->draw.DrawLine( p1, p2, color1 );
			m_context->draw.DrawPoint( p1, 4.0f, color1 );
		}

		// chain-segment vs circle
		if ( m_shapeType == e_circleShape )
		{
			b2Circle circle = { { 0.0f, 0.0f }, 0.5f };
			m_context->draw.DrawSolidCircle( transform2, circle.center, circle.radius, color2 );

			for ( int i = 0; i < m_count; ++i )
			{
				const b2ChainSegment* segment = m_segments + i;
				b2Manifold m = b2CollideChainSegmentAndCircle( segment, transform1, &circle, transform2 );
				DrawManifold( &m );
			}
		}
		else if ( m_shapeType == e_boxShape )
		{
			float h = 0.5f - m_round;
			b2Polygon rox = b2MakeRoundedBox( h, h, m_round );
			m_context->draw.DrawSolidPolygon( transform2, rox.vertices, rox.count, rox.radius, color2 );

			for ( int i = 0; i < m_count; ++i )
			{
				const b2ChainSegment* segment = m_segments + i;
				b2SimplexCache cache = {};
				b2Manifold m = b2CollideChainSegmentAndPolygon( segment, transform1, &rox, transform2, &cache );
				DrawManifold( &m );
			}
		}
	}

	static Sample* Create( SampleContext* context )
	{
		return new SmoothManifold( context );
	}

	ShapeType m_shapeType;

	b2ChainSegment* m_segments;
	int m_count;

	b2Transform m_transform;
	float m_angle;
	float m_round;

	b2Vec2 m_basePosition;
	b2Vec2 m_startPoint;
	float m_baseAngle;

	bool m_dragging;
	bool m_rotating;
	bool m_showIds;
	bool m_showAnchors;
	bool m_showSeparation;
};

static int sampleSmoothManifoldIndex = RegisterSample( "Collision", "Smooth Manifold", SmoothManifold::Create );

class ShapeCast : public Sample
{
public:
	enum ShapeType
	{
		e_point,
		e_segment,
		e_triangle,
		e_box
	};

	explicit ShapeCast( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 0.0f, 0.25f };
			m_context->camera.m_zoom = 3.0f;
		}

		m_point = b2Vec2_zero;
		m_segment = { { 0.0f, 0.0f }, { 0.5f, 0.0f } };

		{
			b2Vec2 points[3] = { { -0.5f, 0.0f }, { 0.5f, 0.0f }, { 0.0f, 1.0f } };
			b2Hull hull = b2ComputeHull( points, 3 );
			m_triangle = b2MakePolygon( &hull, 0.0f );
		}

#if 0
		{
			b2Vec2 points[4] = {};
			points[0].x = -0.599999964;
			points[0].y = -0.700000048;
			points[1].x = 0.449999988;
			points[1].y = -0.700000048;
			points[2].x = 0.449999988;
			points[2].y = 0.350000024;
			points[3].x = -0.599999964;
			points[3].y = 0.350000024;
			b2Hull hull = b2ComputeHull( points, 4 );
			m_triangle = b2MakePolygon( &hull, 0.0f );
		}
#endif

		m_box = b2MakeOffsetBox( 0.5f, 0.5f, { 0.0f, 0.0f }, b2Rot_identity );

#if 0
		{
			b2Vec2 points[4] = {};
			points[0].x = 0.449999988;
			points[0].y = -0.100000001;
			points[1].x = 0.550000012;
			points[1].y = -0.100000001;
			points[2].x = 0.550000012;
			points[2].y = 0.100000001;
			points[3].x = 0.449999988;
			points[3].y = 0.100000001;
			b2Hull hull = b2ComputeHull( points, 4 );
			m_box = b2MakePolygon( &hull, 0.0f );
		}
#endif

		m_transform = { { -0.6f, 0.0f }, b2Rot_identity };
		m_translation = { 2.0f, 0.0f };
		m_angle = 0.0f;
		m_startPoint = { 0.0f, 0.0f };
		m_basePosition = { 0.0f, 0.0f };
		m_baseAngle = 0.0f;

		m_dragging = false;
		m_sweeping = false;
		m_rotating = false;
		m_showIndices = false;
		m_drawSimplex = false;
		m_encroach = false;

		m_typeA = e_box;
		m_typeB = e_point;
		m_radiusA = 0.0f;
		m_radiusB = 0.2f;

		m_proxyA = MakeProxy( m_typeA, m_radiusA );
		m_proxyB = MakeProxy( m_typeB, m_radiusB );
	}

	b2ShapeProxy MakeProxy( ShapeType type, float radius ) const
	{
		b2ShapeProxy proxy = {};
		proxy.radius = radius;

		switch ( type )
		{
			case e_point:
				proxy.points[0] = b2Vec2_zero;
				proxy.count = 1;
				break;

			case e_segment:
				proxy.points[0] = m_segment.point1;
				proxy.points[1] = m_segment.point2;
				proxy.count = 2;
				break;

			case e_triangle:
				for ( int i = 0; i < m_triangle.count; ++i )
				{
					proxy.points[i] = m_triangle.vertices[i];
				}
				proxy.count = m_triangle.count;
				break;

			case e_box:
				proxy.points[0] = m_box.vertices[0];
				proxy.points[1] = m_box.vertices[1];
				proxy.points[2] = m_box.vertices[2];
				proxy.points[3] = m_box.vertices[3];
				proxy.count = 4;
				break;

			default:
				assert( false );
		}

		return proxy;
	}

	void DrawShape( ShapeType type, b2Transform transform, float radius, b2HexColor color )
	{
		switch ( type )
		{
			case e_point:
			{
				b2Vec2 p = b2TransformPoint( transform, m_point );
				if ( radius > 0.0f )
				{
					m_context->draw.DrawSolidCircle( transform, m_point, radius, color );
				}
				else
				{
					m_context->draw.DrawPoint( p, 5.0f, color );
				}
			}
			break;

			case e_segment:
			{
				b2Vec2 p1 = b2TransformPoint( transform, m_segment.point1 );
				b2Vec2 p2 = b2TransformPoint( transform, m_segment.point2 );

				if ( radius > 0.0f )
				{
					m_context->draw.DrawSolidCapsule( p1, p2, radius, color );
				}
				else
				{
					m_context->draw.DrawLine( p1, p2, color );
				}
			}
			break;

			case e_triangle:
				m_context->draw.DrawSolidPolygon( transform, m_triangle.vertices, m_triangle.count, radius, color );
				break;

			case e_box:
				m_context->draw.DrawSolidPolygon( transform, m_box.vertices, m_box.count, radius, color );
				break;

			default:
				assert( false );
		}
	}

	void MouseDown( b2Vec2 p, int button, int mods ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			if ( mods == 0 )
			{
				m_dragging = true;
				m_sweeping = false;
				m_rotating = false;
				m_startPoint = p;
				m_basePosition = m_transform.p;
			}
			else if ( mods == GLFW_MOD_SHIFT )
			{
				m_dragging = false;
				m_sweeping = false;
				m_rotating = true;
				m_startPoint = p;
				m_baseAngle = m_angle;
			}
			else if ( mods == GLFW_MOD_CONTROL )
			{
				m_dragging = false;
				m_sweeping = true;
				m_rotating = false;
				m_startPoint = p;
				m_basePosition = b2Vec2_zero;
			}
		}
	}

	void MouseUp( b2Vec2, int button ) override
	{
		if ( button == GLFW_MOUSE_BUTTON_1 )
		{
			m_dragging = false;
			m_sweeping = false;
			m_rotating = false;
		}
	}

	void MouseMove( b2Vec2 p ) override
	{
		if ( m_dragging )
		{
			m_transform.p = m_basePosition + 0.5f * ( p - m_startPoint );
		}
		else if ( m_rotating )
		{
			float dx = p.x - m_startPoint.x;
			m_angle = b2ClampFloat( m_baseAngle + 1.0f * dx, -B2_PI, B2_PI );
			m_transform.q = b2MakeRot( m_angle );
		}
		else if ( m_sweeping )
		{
			m_translation = p - m_startPoint;
		}
	}

	void UpdateGui() override
	{
		float fontSize = ImGui::GetFontSize();
		float height = 300.0f;
		ImGui::SetNextWindowPos( ImVec2( 0.5f * fontSize, m_camera->m_height - height - 2.0f * fontSize ), ImGuiCond_Once );
		ImGui::SetNextWindowSize( ImVec2( 240.0f, height ) );

		ImGui::Begin( "Shape Distance", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );

		const char* shapeTypes[] = { "point", "segment", "triangle", "box" };
		int shapeType = int( m_typeA );
		if ( ImGui::Combo( "shape A", &shapeType, shapeTypes, IM_ARRAYSIZE( shapeTypes ) ) )
		{
			m_typeA = ShapeType( shapeType );
			m_proxyA = MakeProxy( m_typeA, m_radiusA );
		}

		if ( ImGui::SliderFloat( "radius A", &m_radiusA, 0.0f, 0.5f, "%.2f" ) )
		{
			m_proxyA.radius = m_radiusA;
		}

		shapeType = int( m_typeB );
		if ( ImGui::Combo( "shape B", &shapeType, shapeTypes, IM_ARRAYSIZE( shapeTypes ) ) )
		{
			m_typeB = ShapeType( shapeType );
			m_proxyB = MakeProxy( m_typeB, m_radiusB );
		}

		if ( ImGui::SliderFloat( "radius B", &m_radiusB, 0.0f, 0.5f, "%.2f" ) )
		{
			m_proxyB.radius = m_radiusB;
		}

		ImGui::Separator();

		ImGui::SliderFloat( "x offset", &m_transform.p.x, -2.0f, 2.0f, "%.2f" );
		ImGui::SliderFloat( "y offset", &m_transform.p.y, -2.0f, 2.0f, "%.2f" );

		if ( ImGui::SliderFloat( "angle", &m_angle, -B2_PI, B2_PI, "%.2f" ) )
		{
			m_transform.q = b2MakeRot( m_angle );
		}

		ImGui::Separator();

		ImGui::Checkbox( "show indices", &m_showIndices );
		ImGui::Checkbox( "encroach", &m_encroach );

		ImGui::End();
	}

	void Step() override
	{
		Sample::Step();

		b2ShapeCastPairInput input = {};
		input.proxyA = m_proxyA;
		input.proxyB = m_proxyB;
		input.transformA = b2Transform_identity;
		input.transformB = m_transform;
		input.translationB = m_translation;
		input.maxFraction = 1.0f;
		input.canEncroach = m_encroach;

		b2CastOutput output = b2ShapeCast( &input );

		b2Transform transform;
		transform.q = m_transform.q;
		transform.p = b2MulAdd( m_transform.p, output.fraction, input.translationB );

		b2DistanceInput distanceInput;
		distanceInput.proxyA = m_proxyA;
		distanceInput.proxyB = m_proxyB;
		distanceInput.transformA = b2Transform_identity;
		distanceInput.transformB = transform;
		distanceInput.useRadii = false;
		b2SimplexCache distanceCache;
		distanceCache.count = 0;
		b2DistanceOutput distanceOutput = b2ShapeDistance( &distanceInput, &distanceCache, nullptr, 0 );

		DrawTextLine( "hit = %s, iterations = %d, fraction = %g, distance = %g", output.hit ? "true" : "false", output.iterations,
					  output.fraction, distanceOutput.distance );

		DrawShape( m_typeA, b2Transform_identity, m_radiusA, b2_colorCyan );
		DrawShape( m_typeB, m_transform, m_radiusB, b2_colorLightGreen );
		b2Transform transform2 = { m_transform.p + m_translation, m_transform.q };
		DrawShape( m_typeB, transform2, m_radiusB, b2_colorIndianRed );

		if ( output.hit )
		{
			DrawShape( m_typeB, transform, m_radiusB, b2_colorPlum );

			if ( output.fraction > 0.0f )
			{
				m_context->draw.DrawPoint( output.point, 5.0f, b2_colorWhite );
				m_context->draw.DrawLine( output.point, output.point + 0.5f * output.normal, b2_colorYellow );
			}
			else
			{
				m_context->draw.DrawPoint( output.point, 5.0f, b2_colorPeru );
			}
		}

		if ( m_showIndices )
		{
			for ( int i = 0; i < m_proxyA.count; ++i )
			{
				b2Vec2 p = m_proxyA.points[i];
				m_context->draw.DrawString( p, " %d", i );
			}

			for ( int i = 0; i < m_proxyB.count; ++i )
			{
				b2Vec2 p = b2TransformPoint( m_transform, m_proxyB.points[i] );
				m_context->draw.DrawString( p, " %d", i );
			}
		}

		DrawTextLine( "mouse button 1: drag" );
		DrawTextLine( "mouse button 1 + shift: rotate" );
		DrawTextLine( "mouse button 1 + control: sweep" );
		DrawTextLine( "distance = %.2f, iterations = %d", distanceOutput.distance, output.iterations );
	}

	static Sample* Create( SampleContext* context )
	{
		return new ShapeCast( context );
	}

	b2Polygon m_box;
	b2Polygon m_triangle;
	b2Vec2 m_point;
	b2Segment m_segment;

	ShapeType m_typeA;
	ShapeType m_typeB;
	float m_radiusA;
	float m_radiusB;
	b2ShapeProxy m_proxyA;
	b2ShapeProxy m_proxyB;

	b2Transform m_transform;
	float m_angle;
	b2Vec2 m_translation;

	b2Vec2 m_basePosition;
	b2Vec2 m_startPoint;
	float m_baseAngle;

	bool m_dragging;
	bool m_sweeping;
	bool m_rotating;
	bool m_showIndices;
	bool m_drawSimplex;
	bool m_encroach;
};

static int sampleShapeCast = RegisterSample( "Collision", "Shape Cast", ShapeCast::Create );

class TimeOfImpact : public Sample
{
public:
	explicit TimeOfImpact( SampleContext* context )
		: Sample( context )
	{
		if ( m_context->restart == false )
		{
			m_context->camera.m_center = { 0.6f, 2.0f };
			m_context->camera.m_center = { -16, 45 };
			m_context->camera.m_zoom = 5.0f;
		}
	}

	static Sample* Create( SampleContext* context )
	{
		return new TimeOfImpact( context );
	}

	void Step() override
	{
		Sample::Step();

		b2Sweep sweepA = {
			b2Vec2_zero, { 0.0f, 0.0f }, { 0.0f, 0.0f }, b2Rot_identity, b2Rot_identity,
		};
		b2Sweep sweepB = {
			b2Vec2_zero,
			{ -15.8332710, 45.3520279 },
			{ -15.8324337, 45.3413048 },
			{ -0.540891349, 0.841092527 },
			{ -0.457797021, 0.889056742 },
		};

		b2TOIInput input;
		input.proxyA = b2MakeProxy( m_verticesA, m_countA, m_radiusA );
		input.proxyB = b2MakeProxy( m_verticesB, m_countB, m_radiusB );
		input.sweepA = sweepA;
		input.sweepB = sweepB;
		input.maxFraction = 1.0f;

		b2TOIOutput output = b2TimeOfImpact( &input );

		DrawTextLine( "toi = %g", output.fraction );

		// m_context->draw.DrawString(5, m_textLine, "max toi iters = %d, max root iters = %d", b2_toiMaxIters,
		//                        b2_toiMaxRootIters);

		b2Vec2 vertices[B2_MAX_POLYGON_VERTICES];

		// Draw A
		b2Transform transformA = b2GetSweepTransform( &sweepA, 0.0f );
		for ( int i = 0; i < m_countA; ++i )
		{
			vertices[i] = b2TransformPoint( transformA, m_verticesA[i] );
		}
		m_context->draw.DrawPolygon( vertices, m_countA, b2_colorGray );

		// Draw B at t = 0
		b2Transform transformB = b2GetSweepTransform( &sweepB, 0.0f );
		for ( int i = 0; i < m_countB; ++i )
		{
			vertices[i] = b2TransformPoint( transformB, m_verticesB[i] );
		}
		m_context->draw.DrawSolidCapsule( vertices[0], vertices[1], m_radiusB, b2_colorGreen );
		// m_context->draw.DrawPolygon( vertices, m_countB, b2_colorGreen );

		// Draw B at t = hit_time
		transformB = b2GetSweepTransform( &sweepB, output.fraction );
		for ( int i = 0; i < m_countB; ++i )
		{
			vertices[i] = b2TransformPoint( transformB, m_verticesB[i] );
		}
		m_context->draw.DrawPolygon( vertices, m_countB, b2_colorOrange );

		// Draw B at t = 1
		transformB = b2GetSweepTransform( &sweepB, 1.0f );
		for ( int i = 0; i < m_countB; ++i )
		{
			vertices[i] = b2TransformPoint( transformB, m_verticesB[i] );
		}
		m_context->draw.DrawSolidCapsule( vertices[0], vertices[1], m_radiusB, b2_colorRed );
		// m_context->draw.DrawPolygon( vertices, m_countB, b2_colorRed );

		if ( output.state == b2_toiStateHit )
		{
			b2DistanceInput distanceInput;
			distanceInput.proxyA = input.proxyA;
			distanceInput.proxyB = input.proxyB;
			distanceInput.transformA = b2GetSweepTransform( &sweepA, output.fraction );
			distanceInput.transformB = b2GetSweepTransform( &sweepB, output.fraction );
			distanceInput.useRadii = false;
			b2SimplexCache cache = { 0 };
			b2DistanceOutput distanceOutput = b2ShapeDistance( &distanceInput, &cache, nullptr, 0 );
			DrawTextLine( "distance = %g", distanceOutput.distance );
		}

#if 0
		for (float t = 0.0f; t < 1.0f; t += 0.1f)
		{
			transformB = b2GetSweepTransform(&sweepB, t);
			for (int i = 0; i < m_countB; ++i)
			{
				vertices[i] = b2TransformPoint(transformB, m_verticesB[i]);
			}
			m_context->draw.DrawPolygon(vertices, m_countB, {0.3f, 0.3f, 0.3f});
		}
#endif
	}

	b2Vec2 m_verticesA[4] = { { -16.25, 44.75 }, { -15.75, 44.75 }, { -15.75, 45.25 }, { -16.25, 45.25 } };
	b2Vec2 m_verticesB[2] = { { 0.0f, -0.125000000f }, { 0.0f, 0.125000000f } };

	int m_countA = ARRAY_COUNT( m_verticesA );
	int m_countB = ARRAY_COUNT( m_verticesB );

	float m_radiusA = 0.0f;
	float m_radiusB = 0.0299999993f;
};

static int sampleTimeOfImpact = RegisterSample( "Collision", "Time of Impact", TimeOfImpact::Create );
